#include "mvApplicationState.h"

#include <vtkLookupTable.h>

#include "mvContours.h"
#include "mvGeometry.h"
#include "ParaView.h"
#include "mvInteractor.h"
#include "mvOutline.h"
#include "mvSlice.h"
#include "mvReader.h"
#include "mvVolume.h"
#include "WidgetHints.h"

class ParaView;

mvApplicationState::mvApplicationState()
  : Superclass(),
    m_colorMap(vtkLookupTable::New()),
    m_contours(new mvContours),
    m_geometry(new mvGeometry),
    m_paraview(new ParaView),
    m_interactor(new mvInteractor),
    m_outline(new mvOutline),
    m_reader(new mvReader),
    m_widgetHints(new WidgetHints()),
    m_slice(new mvSlice()),
    m_volume(new mvVolume())
{
  m_objects.push_back(m_contours);
  m_objects.push_back(m_geometry);
  m_objects.push_back(m_paraview);
  m_objects.push_back(m_outline);
  m_objects.push_back(m_slice);
  m_objects.push_back(m_volume);
}

mvApplicationState::~mvApplicationState()
{
  m_colorMap->Delete();
  delete m_contours;
  delete m_geometry;
  delete m_interactor;
  delete m_outline;
  delete m_reader;
  delete m_slice;
  delete m_volume;
  delete m_widgetHints;
}

void mvApplicationState::init()
{
  this->Superclass::init();

  // Not a GLObject, but needs some post-VRUI initialization.
  m_interactor->init();
}
