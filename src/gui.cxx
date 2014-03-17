/*
 * Author: Harry van Haaren 2013
 *         harryhaaren@gmail.com
 * 
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gui.hxx"
#include "avtk/avtk_image.h"
#include "avtk/avtk_button.h"
#include <sstream>

#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

// ONLY to be used for QUIT!
#include "jack.hxx"
extern Jack* jack;

#include "audiobuffer.hxx"
#include "controller/nonseq.hxx"
#include "controller/genericmidi.hxx"


#include <stdlib.h>
#include <FL/Fl.H>
#include <FL/fl_ask.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Wizard.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Multiline_Output.H>

// include the header.c file in the planning dir:
// its the GIMP .c export of the LUPPP header image 
#include "../planning/header.c"
#include "../planning/luppp.c"
#include "../planning/bg.c"

#define GTRACK_WIDTH 110
#define GTRACK_HEIGHT 600

#define MIDI_LED_ARRAY_WIDTH 8
#define MIDI_LED_ARRAY_HEIGHT 5

// Hack, move to gtrack.cpp
int GTrack::privateID = 0;
int GMasterTrack::privateID = 0;
//int AudioBuffer::privateID = 0;

using namespace std;

extern Gui* gui;
extern int signalHanlderInt;

static void signalChecker(void*)
{
  if ( signalHanlderInt )
  {
    // Luppp recieved either a SIGTERM or SIGINT: quit gracefully
    gui->quit();
  }
  else
  {
    Fl::repeat_timeout( 0.1, (Fl_Timeout_Handler)&signalChecker, 0 );
  }
}

void close_cb(Fl_Widget*o, void*)
{
  if ( (Fl::event() == FL_KEYDOWN || Fl::event() == FL_SHORTCUT) && Fl::event_key() == FL_Escape)
  {
    // on excape, as about quitting
    gui->askQuit();
  }
  else
  {
    gui->quit();
  }
}
static void gui_static_read_rb(void* inst)
{
  handleGuiEvents();
  
  Fl::repeat_timeout( 1 / 30.f, &gui_static_read_rb, inst);
}

static void gui_static_nsm_cb(void* inst)
{
  nsm_check_nowait( gui->getNsm() );
  Fl::repeat_timeout( 1 / 30.f, &gui_static_nsm_cb, inst);
}

void option_controller_cb(Fl_Widget*,void* data)
{
  LUPPP_NOTE("%s","Controller cb");
}

static void gui_header_callback(Fl_Widget *w, void *data)
{
  Gui* g = (Gui*)data;
  if ( Fl::event_x() > 130 )
  {
    return;
  }
  
  Fl_Menu_Item rclick_menu[] =
  {
    { "New Session" },
    { "Load Session" },
    { "Save Session   ", 0, 0, 0, FL_MENU_DIVIDER},
    { "Setup", 0, 0, 0, FL_MENU_DIVIDER},
    { "Quit" },
    { 0 }
  };
  
  // check for NSM running, deactivate new / save / load if yes
  if ( gui->getNsm() )
  {
    rclick_menu[1].deactivate();
    rclick_menu[2].deactivate();
  }
  
  Fl_Menu_Item *m = (Fl_Menu_Item*) rclick_menu->popup( 10, 38, 0, 0, 0);
  
  if ( !m )
  {
      return;
  }
  else if ( strcmp(m->label(), "New Session") == 0 )
  {
    int yes = fl_choice("Start a new session?","Cancel","Yes",0);
    if ( yes )
    {
      gui->reset();
    }
  }
  else if ( strcmp(m->label(), "Load Session") == 0 )
  {
    
    std::string tmp;
    {
      // Create the file chooser, and show it
      Fl_File_Chooser chooser(getenv("HOME"),
                              "*",
                              Fl_File_Chooser::DIRECTORY,
                              "Load Session");
      chooser.show();

      // Block until user picks something.
      //     (The other way to do this is to use a callback())
      //
      while(chooser.shown())
          { Fl::wait(); }

      // User hit cancel?
      if ( chooser.value() == NULL )
          { fprintf(stderr, "(User hit 'Cancel')\n"); return; }
      
      // Print what the user picked
      fprintf(stderr, "--------------------\n");
      fprintf(stderr, "DIRECTORY: '%s'\n", chooser.directory());
      fprintf(stderr, "    VALUE: '%s'\n", chooser.value());
      fprintf(stderr, "    COUNT: %d files selected\n", chooser.count());
      
      tmp = chooser.value();
      chooser.hide();
      
      /*
      // try to make the Load window dissapear *now*
      Fl::check();
      Fl::flush();
      Fl::wait(0);
      */
    }
    
    LUPPP_NOTE( "Loading session from dir %s", tmp.c_str() );
    
    // clear the current session: just do a state reset
    EventStateReset e;
    writeToDspRingbuffer( &e );
    
    int sess = gui->getDiskReader()->readSession( tmp );
    if ( sess != LUPPP_RETURN_OK )
      LUPPP_ERROR( "Error loading session" );
    
    return;
  }
  else if ( strcmp(m->label(), "Save Session   ") == 0 )
  {
    const char* name = fl_input( "Save session as", gui->getDiskWriter()->getLastSaveName().c_str() );
    if ( name )
    {
      gui->getDiskWriter()->initialize( getenv("HOME"), name );
      LUPPP_NOTE("%s %s","Saving session as ", name );
      EventStateSave e;
      writeToDspRingbuffer( &e );
    }
  }
  else if ( strcmp(m->label(), "Setup") == 0 )
  {
    g->getOptionsWindow()->show();
  }
  else if ( strcmp(m->label(), "Quit") == 0 )
  {
    g->askQuit();
  }
}

OptionsWindow* Gui::getOptionsWindow()
{
  return optionWindow;
}

/*
void Gui::setMidiControllerLayoutValues(Controller *c)
{
	gridFrameWidth = c->gridWidth;
	gridFrameHeight = c->gridHeight;
}
*/
#if 0
void Gui::selectLoadController(Fl_Widget* w, void*)
{
  // FIXME: refactor
  string path;
  Fl_Native_File_Chooser fnfc;
  fnfc.title("Pick a controller definition");
  fnfc.type(Fl_Native_File_Chooser::BROWSE_FILE);
  fnfc.filter("Controllers\t*.ctlr");
  fnfc.directory( getenv("HOME") ); // default directory to use
  // Show native chooser
  switch ( fnfc.show() ) {
     case -1: printf("ERROR: %s\n", fnfc.errmsg());    break;  // ERROR
     case  1: printf("CANCEL\n");                      break;  // CANCEL
     default: printf("Loading controller at %s\n", fnfc.filename());
        path = fnfc.filename();
        break;
  }
  
  if ( strcmp( path.c_str(), "" ) == 0 )
    return;
  
  LUPPP_NOTE("%s","ADD Controller cb");
  Controller* c = new GenericMIDI( path );
  
  if ( c->status() == Controller::CONTROLLER_OK )
  {
	LUPPP_NOTE("Controller Loaded: GridWidth: %i, GridHeight: %i", c->gridWidth, c->gridHeight);
	if (c->gridWidth > 0 && c->gridHeight > 0) {
		LUPPP_NOTE("Controller has Grid, Show Frame");
        LUPPP_NOTE("Pushing Controller Instance to GUI RB");
        EventControllerConnected tmp(c);
		writeToGuiRingbuffer( &tmp );
	}
    EventControllerInstance e(c);
    writeToDspRingbuffer( &e );
  }
  else
  {
    LUPPP_ERROR("Controller initialization failed!");
  }
  
}
#endif

void Gui::selectLoadSample( int track, int scene )
{
  // FIXME: refactor
  string path;
  Fl_Native_File_Chooser fnfc;
  fnfc.title("Pick a file");
  fnfc.type(Fl_Native_File_Chooser::BROWSE_FILE);
  
  fnfc.filter("Audio\t{*.aiff,*.wav,*.flac,*.ogg}");
  
  std::string defLoadPath = gui->getDiskReader()->getLastLoadedSamplePath();
  fnfc.directory( defLoadPath.c_str() ); // default directory to use
  
  // Show native chooser
  switch ( fnfc.show() ) {
     case -1: /*printf("ERROR: %s\n", fnfc.errmsg()); */ break;  // ERROR
     case  1: /*(printf("CANCEL\n");                  */ break;  // CANCEL
     default: /*printf("Loading directory: %s\n", fnfc.filename()); */
        path = fnfc.filename();
        break;
  }
  
  if ( strcmp( path.c_str(), "" ) == 0 )
    return;
  
  // diskReader loads sample, and parses for sample.cfg
  diskReader->loadSample( track, scene, path );
}

AudioEditor* Gui::getAudioEditor()
{
  return audioEditor;
}
bool Gui::moveGridFrame(int xOffset, int yOffset)
{
	gridFrameTrackOffset += xOffset;
	int numClips = tracks.at(0)->getClipSelector()->numClips;

	LUPPP_NOTE("gridFrameClipOffset: %i, yOffset: %i, numTracks: %i", gridFrameClipOffset, yOffset, int(tracks.size()));
	if ((gridFrameClipOffset + yOffset >= 0) && ((gridFrameClipOffset + gridFrameHeight + yOffset) <= numClips)) {
    	gridFrameClipOffset += yOffset;

		ctrlClipFrame->hide();
		setCtrlFrameSize();
		ctrlClipFrame->show();
		ctrlClipFrame->redraw();
	}
    return true;
}


static int cb_nsm_open (const char *name,
                        const char *display_name,
                        const char *client_id,
                        char **out_msg,
                        void *userdata )
{
  LUPPP_NOTE("NSM: Open, displayname: %s", display_name );
  
  Jack::setup( client_id );
  
  // we *must* get the samplerate here: loading files depends on this information!
  gui->samplerate = jack->getSamplerate();
  
  stringstream loadPath;
  loadPath << name;
  
  // load the NSM provided directory
  gui->getDiskReader()->readSession( loadPath.str() );
  
  // initialize the disk-writer to the same directory:
  // we *always* overwrite the old save file when using NSM
  gui->getDiskWriter()->initialize( name, display_name );
  
  return ERR_OK;
}

static int cb_nsm_save ( char **out_msg, void *userdata )
{
  LUPPP_NOTE("NSM: saving..." );
  
  // disk-writer already initialized to the right directory, so just write!
  EventStateSave e;
  writeToDspRingbuffer( &e );
  
  return 0;
}
void Gui::setCtrlFrameSize() {

	/* Added Frame to show active clips displayed on hardware control */
	int clipHeight = gui->getTrack( 0 )->clipSel.h / gui->getTrack(0)->clipSel.numClips;
	int frameAnchorX = gui->getTrack( 0 )->x();
	int frameAnchorY = gui->getTrack( 0 )->clipSel.y -1 + (clipHeight * gridFrameClipOffset);
	int frameWidth = ((GTRACK_WIDTH+8) * gridFrameWidth) - 8;
	int frameHeight = ((clipHeight) * gridFrameHeight) + 1;
	LUPPP_NOTE("faX: %i, faY: %i, fW: %i, fH: %i, cO: %i", frameAnchorX, frameAnchorY, frameWidth, frameHeight, gridFrameClipOffset);

	ctrlClipFrame->resize(frameAnchorX, frameAnchorY, frameWidth, frameHeight);

}



Gui::Gui(const char* argZero) :
    samplerate( 0 ),
    window(1110,650),
    diskReader( new DiskReader() ),
    diskWriter( new DiskWriter() )
{
  LUPPP_NOTE( "%s", "Gui()" );
  
  gui = this;
  
  // setup callback to signalChecker()
  Fl::add_timeout( 0.1, (Fl_Timeout_Handler)&signalChecker, 0 );
  
  // add keyboard shortcut handler
  Fl::add_handler( keyboardHandler );
  
  //window.resize( false );
  window.xclass("luppp");
  window.iconlabel("luppp");
  
  window.color(FL_BLACK);
  window.label("Luppp");
  window.callback( close_cb, 0 );
  window.size_range( 800, 450 );
  
  window.color( fl_rgb_color (7,7,7) );
  
  /* For Midi Controllers with Grid Pads */
  gridFrameWidth = 0;
  gridFrameHeight = 0;

  /*
  tooltipLabel = new Fl_2(130, 25, 500, 20, "");
  tooltipLabel->labelcolor( FL_LIGHT2 );
  tooltipLabel->color( FL_DARK2 );
  tooltipLabel->hide();
  //tooltipLabel->align( FL_ALIGN_TOP_LEFT );
  */
  
  // horizontal no-resize-images group
  Fl_Group* headerImages = new Fl_Group( 0, 0, 1110, 650, "header");
  {
    //Fl_Pack* vHeaderImages = new Fl_Pack( 0, 0, 1110, 650, "verticalHeader");
    {
      Avtk::Image* bgImage = new Avtk::Image(0,0,1920,36,"bg");
      bgImage->setPixbuf( bgImg.pixel_data, 4 );
      
      Avtk::Image* lupppImage = new Avtk::Image(0,0,130,36,"luppp");
      lupppImage->setPixbuf( lupppImg.pixel_data, 4 );
      lupppImage->callback( gui_header_callback, this );
      
      Avtk::Image* headerImage = new Avtk::Image( window.w() - 270,0,270,36,"header");
      headerImage->setPixbuf( header.pixel_data, 4 );
      headerImage->stickToRight = true;
      
      
      Fl_Box* box = new Fl_Box( 130, 0, 400, 36 );
      headerImages->resizable( box );
      
      //vHeaderImages->set_vertical();
      //vHeaderImages->relayout();
    }
    //vHeaderImages->end();
    
    Fl_Box* box = new Fl_Box( 130, 0, 400, 36 );
    headerImages->resizable( box );
  }
  headerImages->end();
  
  // create a new "Group" with all Luppp GUI contents, for resizing
  lupppGroup = new Fl_Group( 0, 0, 1110, 650, "Luppp");
  {
    // everything in here will have resize() called when main window is resized
    
    /*
    Fl_Bitmap* headImg = new Fl_Bitmap( (unsigned char*)header.pixel_data, 1110, 36 );
    
    Fl_Box* pic_box = new Fl_Box(0,0,1110,36);
    pic_box->image( headImg );
    pic_box->redraw();
    */
    
    int i = 0;
    for (; i < NTRACKS; i++ )
    {
      stringstream s;
      s << "Track " << i+1;
      //printf("track name %s\n", s.str().c_str() );
      tracks.push_back( new GTrack(8 + i * 118, 40, GTRACK_WIDTH, GTRACK_HEIGHT, s.str().c_str() ) );
    }
    
    master = new GMasterTrack(8 + i * 118, 40, 150, 600, "Master");

		ctrlClipFrame = new Fl_Box(
				FL_BORDER_FRAME,
				0,
				0,
				0,
				0,
				NULL
			);

		  ctrlClipFrame->color(FL_GREEN);
    }

  /* ctrlClipFrame->hide(); */

  lupppGroup->end();
  
  window.resizable( lupppGroup );
  
  window.end();
  

  /* tracks.at(0)->getClipSelector()->setSpecial( 0 ); */

  optionWindow = new OptionsWindow();

  // Create AudioEditor after window.end() has been called
  audioEditor = new AudioEditor();

  // read settings file using diskreader, and setup controllers etc
  int prefs = diskReader->loadPreferences();
  if ( prefs != LUPPP_RETURN_OK )
  {
    LUPPP_WARN("No preferences loaded, using defaults.");
  }
  else
  {
    LUPPP_NOTE("Loaded preferences");
  }
  
  // NSM stuff
  nsm = 0;
  const char *nsm_url = getenv( "NSM_URL" );
  if ( nsm_url )
  {
    nsm = nsm_new();
    
    nsm_set_open_callback( nsm, cb_nsm_open, this );
    nsm_set_save_callback( nsm, cb_nsm_save, this );
    
    if ( nsm_init( nsm, nsm_url ) == 0 )
    {
      nsm_send_announce( nsm, "Luppp", "", argZero );
      LUPPP_NOTE("Announcing to NSM");
    }
    else
    {
      nsm_free( nsm );
      nsm = 0;
    }
  }
  else
  {
    LUPPP_NOTE("No session management in use");
  }
}

void Gui::addMidiControllerToSetup(std::string c)
{
  controllerVector.push_back( c );
}

void Gui::setupMidiControllers()
{
  for(unsigned int i = 0; i < controllerVector.size(); i++)
  {
    GenericMIDI* c = new GenericMIDI( controllerVector.at(i).c_str() );
    if ( c )
    {
      EventControllerInstance e(c);
      writeToDspRingbuffer( &e );
    }
  }
}

void Gui::reset()
{
  // signal to DSP to reset state
  EventStateReset ev;
  writeToDspRingbuffer( &ev );
  
  // clear UI state: track names / scene names
  for(unsigned int i = 0; i < NTRACKS; i++)
  {
    stringstream s;
    s << "Track " << i+1;
    tracks.at(i)->bg.setLabel( s.str().c_str() );
    
    for(unsigned int s = 0; s < NSCENES; s++)
    {
      tracks.at(i)->getClipSelector()->clipName( s, "" );
    }
  }
  
  for(unsigned int i = 0; i < NSCENES; i++)
  {
    stringstream s;
    s << "Scene " << i+1;
    master->getClipSelector()->clipName( i, s.str() );
  }
  
}

GTrack* Gui::getTrack(int id)
{
  return tracks.at(id);
}

int Gui::show()
{
  window.show();
  
  gui_static_read_rb( this );
  
  // only launch NSM check callbacks if NSM is setup
  if ( gui->getNsm() )
    gui_static_nsm_cb( this );
  
  return Fl::run();
}


int Gui::quit()
{
  // close JACK client
  jack->quit();
  
  // ensure the subwindows are closed
  optionWindow->hide();
  audioEditor->hide();
  
  // quit main window, causing program termination
  window.hide();
  
  return 0;
}

void Gui::askQuit()
{
  int quit = fl_choice("Really Quit?","Cancel","Quit",0);
  if ( quit ) // JUST QUIT
  {
    gui->quit();
  }
}

int Gui::keyboardHandler(int event)
{
  printf("event\n");
  
  switch( event )
  {
    case FL_SHORTCUT:
      if     ( strcmp( Fl::event_text(), "1" ) == 0 ) { EventGridEvent e( 0, 0, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "2" ) == 0 ) { EventGridEvent e( 1, 0, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "3" ) == 0 ) { EventGridEvent e( 2, 0, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "4" ) == 0 ) { EventGridEvent e( 3, 0, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "5" ) == 0 ) { EventGridEvent e( 4, 0, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "6" ) == 0 ) { EventGridEvent e( 5, 0, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "7" ) == 0 ) { EventGridEvent e( 6, 0, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "8" ) == 0 ) { EventGridEvent e( 7, 0, true ); writeToDspRingbuffer( &e ); return 1; }
      
      else if( strcmp( Fl::event_text(), "q" ) == 0 ) { EventGridEvent e( 0, 1, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "w" ) == 0 ) { EventGridEvent e( 1, 1, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "e" ) == 0 ) { EventGridEvent e( 2, 1, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "r" ) == 0 ) { EventGridEvent e( 3, 1, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "t" ) == 0 ) { EventGridEvent e( 4, 1, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "y" ) == 0 ) { EventGridEvent e( 5, 1, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "u" ) == 0 ) { EventGridEvent e( 6, 1, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "i" ) == 0 ) { EventGridEvent e( 7, 1, true ); writeToDspRingbuffer( &e ); return 1; }
      
      else if( strcmp( Fl::event_text(), "a" ) == 0 ) { EventGridEvent e( 0, 2, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "s" ) == 0 ) { EventGridEvent e( 1, 2, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "d" ) == 0 ) { EventGridEvent e( 2, 2, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "f" ) == 0 ) { EventGridEvent e( 3, 2, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "g" ) == 0 ) { EventGridEvent e( 4, 2, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "h" ) == 0 ) { EventGridEvent e( 5, 2, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "j" ) == 0 ) { EventGridEvent e( 6, 2, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "k" ) == 0 ) { EventGridEvent e( 7, 2, true ); writeToDspRingbuffer( &e ); return 1; }
      
      else if( strcmp( Fl::event_text(), "z" ) == 0 ) { EventGridEvent e( 0, 3, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "x" ) == 0 ) { EventGridEvent e( 1, 3, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "c" ) == 0 ) { EventGridEvent e( 2, 3, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "v" ) == 0 ) { EventGridEvent e( 3, 3, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "b" ) == 0 ) { EventGridEvent e( 4, 3, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "n" ) == 0 ) { EventGridEvent e( 5, 3, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "m" ) == 0 ) { EventGridEvent e( 6, 3, true ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "," ) == 0 ) { EventGridEvent e( 7, 3, true ); writeToDspRingbuffer( &e ); return 1; }
      
      else if( strcmp( Fl::event_text(), "9" ) == 0 ) { EventGridLaunchScene e( 0 ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "o" ) == 0 ) { EventGridLaunchScene e( 1 ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "l" ) == 0 ) { EventGridLaunchScene e( 2 ); writeToDspRingbuffer( &e ); return 1; }
      else if( strcmp( Fl::event_text(), "." ) == 0 ) { EventGridLaunchScene e( 3 ); writeToDspRingbuffer( &e ); return 1; }
      
      else
      {
        //printf("%s\n", Fl::event_text() ); return 1;
      }
  }
  
  return 0;
}


Gui::~Gui()
{
  delete optionWindow;
  delete audioEditor;
  
  delete diskReader;
  delete diskWriter;
  
  delete master;
  
  for(unsigned int i = 0; i < tracks.size(); i++)
  {
    delete tracks.at(i);
  }
  
  delete ctrlClipFrame;
  
}
