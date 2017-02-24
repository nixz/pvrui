#ifndef PARAVIEW_H
#define PARAVIEW_H

#include "vvLODAsyncGLObject.h"

#include <vtkNew.h>
#include <vtkSmartPointer.h>

class vtkActor;
class vtkActorCollection;
class vtkCompositeDataGeometryFilter;
class vtkDataObject;
class vtkPolyDataMapper;
class vtkSMRenderViewProxy;


/**
 * @brief The ParaView class renders the dataset as polydata.
 */
class ParaView : public vvLODAsyncGLObject
{
public:
  using Superclass = vvLODAsyncGLObject;

  enum Representation
    {
    NoGeometry,
    Points,
    Wireframe,
    Surface,
    SurfaceWithEdges
    };

  struct GeometryState : public Superclass::ObjectState
  {
    double opacity{1.};
    Representation representation{Surface};
    bool visible{true};

    void update(const vvApplicationState &state) override {}
  };

  // LoRes LOD: ----------------------------------------------------------------
  // Run vtkCompositeDataGeometryFilter on the reduced dataset:
  struct LoResDataPipeline : public Superclass::DataPipeline
  {
    vtkNew<vtkCompositeDataGeometryFilter> filter;

    // Returns the dataset to use. This is the only difference between the
    // LoRes and HiRes pipelines, so this should save some duplication.
    virtual vtkDataObject* input(const vvApplicationState &state) const;

    void configure(const ObjectState &, const vvApplicationState &) override;
    bool needsUpdate(const ObjectState &objState,
                     const LODData &result) const override;
    void execute() override;
    void exportResult(LODData &result) const override;
  };

  // HiRes LOD: ----------------------------------------------------------------
  // Run vtkCompositeDataGeometryFilter on the full dataset:
  struct HiResDataPipeline : public LoResDataPipeline
  {
    vtkDataObject* input(const vvApplicationState &state) const override;
  };

  // Shared: LODData and RenderPipeline are shared between LoRes and HiRes. ----
  struct GeometryLODData : public Superclass::LODData
  {
    vtkSmartPointer<vtkDataObject> geometry;
  };

  struct GeometryRenderPipeline : public Superclass::RenderPipeline
  {
    vtkNew<vtkPolyDataMapper> mapper;
    vtkNew<vtkActor> actor;
    vtkNew<vtkActorCollection> actors;

    void init(const ObjectState &objState,
              vvContextState &contextState) override;
    void update(const ObjectState &objState,
                const vvApplicationState &appState,
                const vvContextState &contextState,
                const LODData &result) override;
    void disable();
  };

  // ParaView API ------------------------------------------------------------
  ParaView();
  ~ParaView();

  /**
   * Toggle visibility of the contour props on/off.
   */
  bool visible() const { return this->objectState<GeometryState>().visible; }
  void setVisible(bool v) { this->objectState<GeometryState>().visible = v; }

  /**
   * The actor's opacity.
   */
  double opacity() const;
  void setOpacity(double opacity);

  /**
   * The polydata representation to use.
   */
  Representation representation() const;
  void setRepresentation(Representation representation);

private: // vvAsyncGLObject virtual API:
  std::string progressLabel() const override { return "PVGeometry"; }

  ObjectState* createObjectState() const override;
  DataPipeline* createDataPipeline(LevelOfDetail lod) const override;
  RenderPipeline* createRenderPipeline(LevelOfDetail lod) const override;
  LODData* createLODData(LevelOfDetail lod) const override;

private:
  // Not implemented -- disable copy:
  ParaView(const ParaView&);
  ParaView& operator=(const ParaView&);
};

#endif // PARAVIEW_H
