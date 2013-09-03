
#include "saveable.hxx"

#include "../jack.hxx"
#include "save.hxx"

extern Jack* jack;

int SaveAble::savesDone = 0;

SaveAble::SaveAble()
{
  jack->getSave()->registerSaveable( this );
}

void SaveAble::save(){};

void SaveAble::done()
{
  savesDone++;
  
  if ( savesDone == jack->getSave()->getNumSaveables() )
  {
    jack->getSave()->finish();
  }
}