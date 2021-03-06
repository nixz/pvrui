#include "mvVolume.h"

#include <vtkColorTransferFunction.h>
#include <vtkCompositeDataIterator.h>
#include <vtkExternalOpenGLRenderer.h>
#include <vtkImageData.h>
#include <vtkLookupTable.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkNew.h>
#include <vtkPiecewiseFunction.h>
#include <vtkResampleToImage.h>
#include <vtkSmartVolumeMapper.h>
#include <vtkVolume.h>
#include <vtkVolumeProperty.h>

#include <GL/GLContextData.h>

#include <vvContextState.h>

#include "mvApplicationState.h"
#include "mvReader.h"

//------------------------------------------------------------------------------
mvVolume::VolumeState::VolumeState()
  : renderMode(vtkSmartVolumeMapper::DefaultRenderMode),
    visible(false),
    dimension(32)
{
}

//------------------------------------------------------------------------------
void mvVolume::LoResDataPipeline::configure(const ObjectState &,
                                            const vvApplicationState &vvState)
{
  const mvApplicationState &appState =
      static_cast<const mvApplicationState &>(vvState);

  // Grab the current data object pointer.
  this->reducedDataObject = appState.reader().reducedDataObject();
}

//------------------------------------------------------------------------------
bool mvVolume::LoResDataPipeline::needsUpdate(const ObjectState &objState,
                                              const LODData &result) const
{
  // Sync the data object pointer with the result object.
  const VolumeLODData &data = static_cast<const VolumeLODData&>(result);
  const VolumeState &state = static_cast<const VolumeState&>(objState);

  if (!state.visible)
    {
    return false;
    }

  // One object exists, the other does -- update:
  if ((data.volume && !this->reducedDataObject) ||
      (!data.volume && this->reducedDataObject))
    {
    return true;
    }

  // Result object is out-of-date:
  if (data.volume && this->reducedDataObject &&
      data.volume->GetMTime() < this->reducedDataObject->GetMTime())
    {
    return true;
    }

  return false;
}

//------------------------------------------------------------------------------
void mvVolume::LoResDataPipeline::exportResult(LODData &result) const
{
  VolumeLODData &data = static_cast<VolumeLODData&>(result);
  if (this->reducedDataObject)
    {
    data.volume.TakeReference(this->reducedDataObject->NewInstance());
    data.volume->ShallowCopy(this->reducedDataObject.Get());
    }
  else
    {
    data.volume = nullptr;
    }
}

//------------------------------------------------------------------------------
mvVolume::VolumeRenderPipeline::VolumeRenderPipeline()
{
  this->property->SetColor(this->color.Get());
  this->property->SetScalarOpacity(this->opacity.Get());
  this->property->SetInterpolationTypeToLinear();
  this->property->ShadeOff();
  this->actor->SetProperty(this->property.Get());
  this->actor->SetMapper(this->mapper.GetPointer());
}

//------------------------------------------------------------------------------
void mvVolume::VolumeRenderPipeline::init(const ObjectState &,
                                          vvContextState &contextState)
{
  contextState.renderer().AddVolume(this->actor.Get());
}

//------------------------------------------------------------------------------
void mvVolume::VolumeRenderPipeline::update(const ObjectState &objState,
                                            const vvApplicationState &vvState,
                                            const vvContextState &contextState,
                                            const LODData &result)
{
  const mvApplicationState &appState =
      static_cast<const mvApplicationState &>(vvState);
  const VolumeState &state = static_cast<const VolumeState&>(objState);
  const VolumeLODData &data = static_cast<const VolumeLODData&>(result);

  // If the volume is a composite dataset, just grab the first leaf.
  // TODO this could just create multiple mappers/actors for each volume if
  // multi-leaf datasets are used.
  vtkImageData *image = nullptr;
  if (vtkCompositeDataSet *cds = vtkCompositeDataSet::SafeDownCast(data.volume))
    {
    vtkCompositeDataIterator *iter = cds->NewIterator();
    for (; !iter->IsDoneWithTraversal(); iter->GoToNextItem())
      {
      if (image = vtkImageData::SafeDownCast(iter->GetCurrentDataObject()))
        {
        break;
        }
      }
    iter->Delete();
    }
  else if (image = vtkImageData::SafeDownCast(data.volume))
    {
    // image holds the vtkImageData pointer.
    }

  auto metaData = appState.reader().variableMetaData(appState.colorByArray());
  if (!image || !state.visible || !metaData.valid() ||
      metaData.location != mvReader::VariableMetaData::Location::PointData)
    {
    this->disable();
    return;
    }

  this->mapper->SetInputDataObject(image);
  this->mapper->SetRequestedRenderMode(state.renderMode);
  this->mapper->SelectScalarArray(appState.colorByArray().c_str());
  this->mapper->SetScalarModeToUsePointFieldData();

  // Sync color tables
  if (appState.colorMap().GetMTime() > std::min(this->color->GetMTime(),
                                                this->opacity->GetMTime()))
    {
    this->color->RemoveAllPoints();
    this->opacity->RemoveAllPoints();
    double step = (metaData.range[1] - metaData.range[0]) / 255.0;
    double rgba[4];
    for (vtkIdType i = 0; i < 256; ++i)
      {
      const double x = metaData.range[0] + (i * step);
      appState.colorMap().GetTableValue(i, rgba);
      this->color->AddRGBPoint(x, rgba[0], rgba[1], rgba[2]);
      this->opacity->AddPoint(x, rgba[3]);
      }
    }

  this->actor->SetVisibility(1);
}

//------------------------------------------------------------------------------
void mvVolume::VolumeRenderPipeline::disable()
{
  this->actor->SetVisibility(0);
}

//------------------------------------------------------------------------------
mvVolume::HiResDataPipeline::HiResDataPipeline()
{
}

//------------------------------------------------------------------------------
void mvVolume::HiResDataPipeline::configure(const ObjectState &objState,
                                            const vvApplicationState &vvState)
{
  const mvApplicationState &appState =
      static_cast<const mvApplicationState &>(vvState);
  const VolumeState &state = static_cast<const VolumeState&>(objState);

  this->filter->SetInputDataObject(appState.reader().dataObject());
  this->filter->SetSamplingDimensions(state.dimension, state.dimension,
                                      state.dimension);

}

//------------------------------------------------------------------------------
bool mvVolume::HiResDataPipeline::needsUpdate(const ObjectState &objState,
                                              const LODData &result) const
{
  const VolumeState &state = static_cast<const VolumeState&>(objState);
  const VolumeLODData &data = static_cast<const VolumeLODData&>(result);

  return
      state.visible &&
      this->filter->GetInputDataObject(0, 0) &&
      (!data.volume ||
       data.volume->GetMTime() < this->filter->GetMTime());
}

//------------------------------------------------------------------------------
void mvVolume::HiResDataPipeline::execute()
{
  this->filter->Update();
}

//------------------------------------------------------------------------------
void mvVolume::HiResDataPipeline::exportResult(LODData &result) const
{
  VolumeLODData &data = static_cast<VolumeLODData&>(result);
  vtkDataObject *dObj = this->filter->GetOutputDataObject(0);
  data.volume.TakeReference(dObj->NewInstance());
  data.volume->ShallowCopy(dObj);
}

//------------------------------------------------------------------------------
mvVolume::mvVolume()
{
}

//------------------------------------------------------------------------------
mvVolume::~mvVolume()
{
}

//------------------------------------------------------------------------------
int mvVolume::renderMode() const
{
  return this->objectState<VolumeState>().renderMode;
}

//------------------------------------------------------------------------------
void mvVolume::setRenderMode(int mode)
{
  this->objectState<VolumeState>().renderMode = mode;
}

//------------------------------------------------------------------------------
double mvVolume::dimension() const
{
  return this->objectState<VolumeState>().dimension;
}

//------------------------------------------------------------------------------
void mvVolume::setDimension(double d)
{
  this->objectState<VolumeState>().dimension = d;
}

//------------------------------------------------------------------------------
vvLODAsyncGLObject::ObjectState* mvVolume::createObjectState() const
{
  return new VolumeState;
}

//------------------------------------------------------------------------------
vvLODAsyncGLObject::DataPipeline*
mvVolume::createDataPipeline(LevelOfDetail lod) const
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
vvLODAsyncGLObject::RenderPipeline*
mvVolume::createRenderPipeline(LevelOfDetail lod) const
{
  switch (lod)
    {
    case LevelOfDetail::Hint:
      return nullptr;

    case LevelOfDetail::LoRes:
    case LevelOfDetail::HiRes:
      return new VolumeRenderPipeline;

    default:
      return nullptr;
    }
}

//------------------------------------------------------------------------------
vvLODAsyncGLObject::LODData*
mvVolume::createLODData(LevelOfDetail lod) const
{
  switch (lod)
    {
    case LevelOfDetail::Hint:
      return nullptr;

    case LevelOfDetail::LoRes:
    case LevelOfDetail::HiRes:
      return new VolumeLODData;

    default:
      return nullptr;
    }
}
