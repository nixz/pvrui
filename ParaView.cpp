#include "ParaView.h"

#include <GL/GLContextData.h>

#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkSMRenderViewProxy.h>
#include <vtkCompositeDataGeometryFilter.h>
#include <vtkExodusIIReader.h>
#include <vtkExternalOpenGLRenderer.h>
#include <vtkLookupTable.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

#include <vvContextState.h>

#include "mvApplicationState.h"
#include "mvReader.h"

extern vtkSMRenderViewProxy* RVP;

//------------------------------------------------------------------------------
vtkDataObject *
ParaView::LoResDataPipeline::input(const vvApplicationState &vvState) const
{
  const mvApplicationState &state =
      static_cast<const mvApplicationState &>(vvState);

  return state.reader().reducedDataObject();
}

//------------------------------------------------------------------------------
void ParaView::LoResDataPipeline::configure(
    const ObjectState &, const vvApplicationState &appState)
{
  this->filter->SetInputDataObject(this->input(appState));
}

//------------------------------------------------------------------------------
bool ParaView::LoResDataPipeline::needsUpdate(const ObjectState &objState,
                                                const LODData &result) const
{
  const GeometryState &state = static_cast<const GeometryState&>(objState);
  const GeometryLODData &data = static_cast<const GeometryLODData&>(result);

  return
      state.visible &&
      state.representation != Representation::NoGeometry &&
      this->filter->GetInputDataObject(0, 0) &&
      (!data.geometry ||
       data.geometry->GetMTime() < this->filter->GetMTime());
}

//------------------------------------------------------------------------------
void ParaView::LoResDataPipeline::execute()
{
  this->filter->Update();
}

//------------------------------------------------------------------------------
void ParaView::LoResDataPipeline::exportResult(LODData &result) const
{
  GeometryLODData &data = static_cast<GeometryLODData&>(result);

  vtkDataObject *dObj = this->filter->GetOutputDataObject(0);
  data.geometry.TakeReference(dObj->NewInstance());
  data.geometry->ShallowCopy(dObj);
}

//------------------------------------------------------------------------------
vtkDataObject *
ParaView::HiResDataPipeline::input(const vvApplicationState &vvState) const
{
  const mvApplicationState &state =
      static_cast<const mvApplicationState &>(vvState);

  return state.reader().dataObject();
}

//------------------------------------------------------------------------------
void ParaView::GeometryRenderPipeline::init(const ObjectState &,
                                              vvContextState &contextState)
{
  this->mapper->SetScalarVisibility(1);
  this->actor->GetProperty()->SetEdgeColor(1., 1., 1.);
  this->actor->SetMapper(this->mapper.Get());

  contextState.renderer().AddActor(this->actor.Get());
  if (RVP){
      vtkActorCollection* totalActors = RVP->GetRenderer()->GetActors();
      vtkActor * pCurActor = NULL;
       totalActors->InitTraversal();
        do
          {
          pCurActor = totalActors->GetNextActor();

          if (pCurActor != NULL && pCurActor->GetVisibility())
            if(pCurActor->GetMapper() !=NULL)
              {
              contextState.renderer().AddActor(pCurActor);
              }

          } while (pCurActor != NULL);
      }
  }

//------------------------------------------------------------------------------
void ParaView::GeometryRenderPipeline::update(
    const ObjectState &objState, const vvApplicationState &vvState,
    const vvContextState &contextState, const LODData &result)
{
  const mvApplicationState &appState =
      static_cast<const mvApplicationState &>(vvState);
  const GeometryState &state = static_cast<const GeometryState&>(objState);
  const GeometryLODData &data = static_cast<const GeometryLODData&>(result);

  if (!state.visible ||
      state.representation == Representation::NoGeometry ||
      !data.geometry)
    {
    this->disable();
    return;
    }

  this->mapper->SetInputDataObject(data.geometry.Get());

  auto metaData = appState.reader().variableMetaData(appState.colorByArray());
  if (metaData.valid())
    {
    switch (metaData.location)
    {
    case mvReader::VariableMetaData::Location::PointData:
      this->mapper->SetScalarModeToUsePointFieldData();
      break;

    case mvReader::VariableMetaData::Location::CellData:
      this->mapper->SetScalarModeToUseCellFieldData();
      break;

    case mvReader::VariableMetaData::Location::FieldData:
      this->mapper->SetScalarModeToUseFieldData();
      break;

    default:
      break;
    }

    this->mapper->SelectColorArray(appState.colorByArray().c_str());
    this->mapper->SetScalarRange(metaData.range);
    this->mapper->SetLookupTable(&appState.colorMap());
    }

  switch (state.representation)
    {
    case ParaView::NoGeometry:
      // Shouldn't happen, we check for this earlier.
      break;
    case ParaView::Points:
      this->actor->GetProperty()->SetRepresentation(VTK_POINTS);
      this->actor->GetProperty()->EdgeVisibilityOff();
      break;
    case ParaView::Wireframe:
      this->actor->GetProperty()->SetRepresentation(VTK_WIREFRAME);
      this->actor->GetProperty()->EdgeVisibilityOff();
      break;
    case ParaView::Surface:
      this->actor->GetProperty()->SetRepresentation(VTK_SURFACE);
      this->actor->GetProperty()->EdgeVisibilityOff();
      break;
    case ParaView::SurfaceWithEdges:
      this->actor->GetProperty()->SetRepresentation(VTK_SURFACE);
      this->actor->GetProperty()->EdgeVisibilityOn();
      break;
    }

  this->actor->GetProperty()->SetOpacity(state.opacity);
  this->actor->SetVisibility(1);
}

//------------------------------------------------------------------------------
void ParaView::GeometryRenderPipeline::disable()
{
  this->actor->SetVisibility(0);
}

//------------------------------------------------------------------------------
ParaView::ParaView()
{
}

//------------------------------------------------------------------------------
ParaView::~ParaView()
{
}

//------------------------------------------------------------------------------
double ParaView::opacity() const
{
  return this->objectState<GeometryState>().opacity;
}

//------------------------------------------------------------------------------
void ParaView::setOpacity(double o)
{
  this->objectState<GeometryState>().opacity = o;
}

//------------------------------------------------------------------------------
ParaView::Representation ParaView::representation() const
{
  return this->objectState<GeometryState>().representation;
}

//------------------------------------------------------------------------------
void ParaView::setRepresentation(Representation repr)
{
  this->objectState<GeometryState>().representation = repr;
}

//------------------------------------------------------------------------------
vvLODAsyncGLObject::ObjectState *ParaView::createObjectState() const
{
  return new GeometryState;
}

//------------------------------------------------------------------------------
vvLODAsyncGLObject::DataPipeline *
ParaView::createDataPipeline(LevelOfDetail lod) const
{
  switch (lod)
    {
    case LevelOfDetail::Hint:
      return nullptr;

    case LevelOfDetail::LoRes:
      return new LoResDataPipeline;

    case LevelOfDetail::HiRes:
      return new HiResDataPipeline;

    default:
      return nullptr;
    }
}

//------------------------------------------------------------------------------
vvLODAsyncGLObject::RenderPipeline *
ParaView::createRenderPipeline(LevelOfDetail lod) const
{
  switch (lod)
    {
    case LevelOfDetail::Hint:
      return nullptr;

    case LevelOfDetail::LoRes:
    case LevelOfDetail::HiRes:
      return new GeometryRenderPipeline;

    default:
      return nullptr;
    }
}

//------------------------------------------------------------------------------
vvLODAsyncGLObject::LODData *
ParaView::createLODData(LevelOfDetail lod) const
{
  switch (lod)
    {
    case LevelOfDetail::Hint:
      return nullptr;

    case LevelOfDetail::LoRes:
    case LevelOfDetail::HiRes:
      return new GeometryLODData;

    default:
      return nullptr;
    }
}
