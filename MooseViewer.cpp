// STL includes
#include <algorithm>
#include <iostream>
#include <math.h>
#include <sstream>

// Must come before any gl.h include
#include <GL/glew.h>

// VTK includes
#include <ExternalVTKWidget.h>
#include <vtkActor.h>
#include <vtkAppendPolyData.h>
#include <vtkCellData.h>
#include <vtkCellDataToPointData.h>
#include <vtkCheckerboardSplatter.h>
#include <vtkColorTransferFunction.h>
#include <vtkCompositeDataGeometryFilter.h>
#include <vtkCompositeDataIterator.h>
#include <vtkCompositePolyDataMapper.h>
#include <vtkSMPContourGrid.h>
#include <vtkCubeSource.h>
#include <vtkExodusIIReader.h>
#include <vtkImageData.h>
#include <vtkLookupTable.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkNew.h>
#include <vtkOutlineFilter.h>
#include <vtkPiecewiseFunction.h>
#include <vtkPointData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkSmartVolumeMapper.h>
#include <vtkTextActor.h>
#include <vtkUnstructuredGrid.h>
#include <vtkSpanSpace.h>

// OpenGL/Motif includes
#include <GL/GLContextData.h>
#include <GL/gl.h>
#include <GLMotif/CascadeButton.h>
#include <GLMotif/Menu.h>
#include <GLMotif/Pager.h>
#include <GLMotif/Popup.h>
#include <GLMotif/PopupMenu.h>
#include <GLMotif/RadioBox.h>
#include <GLMotif/Separator.h>
#include <GLMotif/ScrolledListBox.h>
#include <GLMotif/StyleSheet.h>
#include <GLMotif/SubMenu.h>
#include <GLMotif/ToggleButton.h>
#include <GLMotif/WidgetManager.h>

// VRUI includes
#include <Vrui/Application.h>
#include <Vrui/Tool.h>
#include <Vrui/ToolManager.h>
#include <Vrui/Vrui.h>
#include <Vrui/VRWindow.h>
#include <Vrui/WindowProperties.h>

// MooseViewer includes
#include "AnimationDialog.h"
#include "ArrayLocator.h"
#include "BaseLocator.h"
#include "ClippingPlane.h"
#include "ClippingPlaneLocator.h"
#include "ColorMap.h"
#include "Contours.h"
#include "DataItem.h"
#include "MooseViewer.h"
#include "ScalarWidget.h"
#include "TransferFunction1D.h"
#include "UnstructuredContourObject.h"
#include "VariablesDialog.h"
#include "WidgetHints.h"

//----------------------------------------------------------------------------
MooseViewer::MooseViewer(int& argc,char**& argv)
  :Vrui::Application(argc,argv),
  analysisTool(0),
  ClippingPlanes(NULL),
  colorByVariablesMenu(0),
  ContoursDialog(NULL),
  ContourVisible(true),
  FirstFrame(true),
  GaussianSplatterDims(30),
  GaussianSplatterExp(-1.0),
  GaussianSplatterRadius(0.01),
  IsPlaying(false),
  Loop(false),
  mainMenu(NULL),
  m_contours(new UnstructuredContourObject),
  NumberOfClippingPlanes(6),
  Opacity(1.0),
  opacityValue(NULL),
  Outline(true),
  renderingDialog(NULL),
  RepresentationType(2),
  RequestedRenderMode(3),
  sampleValue(NULL),
  ShowFPS(false),
  variablesDialog(0),
  Volume(false),
  widgetHints(new WidgetHints)
{
  /* Set Window properties:
   * Since the application requires translucency, GLX_ALPHA_SIZE is set to 1 at
   * context (VRWindow) creation time. To do this, we set the 4th component of
   * ColorBufferSize in WindowProperties to 1. This should be done in the
   * constructor to make sure it is set before the main loop is called.
   */
  Vrui::WindowProperties properties;
  properties.setColorBufferSize(0,1);
  Vrui::requestWindowProperties(properties);

  // Children must be initialized before MooseViewer so that syncContext will
  // work.
  this->dependsOn(m_contours);
}

//----------------------------------------------------------------------------
MooseViewer::~MooseViewer(void)
{
  delete[] this->ColorMap;
  delete[] this->ClippingPlanes;
  delete[] this->DataBounds;
  delete[] this->Histogram;
  delete[] this->ScalarRange;

  delete this->AnimationControl;
  delete this->ColorEditor;
  delete this->ContoursDialog;
  delete this->mainMenu;
  delete this->m_contours;
  delete this->renderingDialog;
  delete this->variablesDialog;
  delete this->widgetHints;
}

//----------------------------------------------------------------------------
void MooseViewer::Initialize()
{
  if (!this->widgetHintsFile.empty())
    {
    this->widgetHints->loadFile(this->widgetHintsFile);
    }
  else
    {
    this->widgetHints->reset();
    }

  /* Create the user interface: */
  this->variablesDialog = new VariablesDialog;
  this->variablesDialog->getScrolledListBox()->getListBox()->
      getSelectionChangedCallbacks().add(
        this, &MooseViewer::changeVariablesCallback);

  renderingDialog = createRenderingDialog();
  mainMenu=createMainMenu();
  Vrui::setMainMenu(mainMenu);

  this->reader = vtkSmartPointer<vtkExodusIIReader>::New();
  this->variables.clear();

  this->DataBounds = new double[6];

  /* Color Map */
  this->ColorMap = new double[4*256];

  /* Histogram */
  this->Histogram = new float[256];
  for(int j = 0; j < 256; ++j)
    {
    this->Histogram[j] = 0.0;
    }

  this->ScalarRange = new double[2];
  this->ScalarRange[0] = 0.0;
  this->ScalarRange[1] = 255.0;

  /* Initialize the clipping planes */
  ClippingPlanes = new ClippingPlane[NumberOfClippingPlanes];
  for(int i = 0; i < NumberOfClippingPlanes; ++i)
    {
    ClippingPlanes[i].setAllocated(false);
    ClippingPlanes[i].setActive(false);
    }

}

//----------------------------------------------------------------------------
void MooseViewer::setFileName(const std::string &name)
{
  if (this->FileName == name)
    {
    return;
    }

  this->FileName = name;

  this->reader->SetFileName(this->FileName.c_str());
  this->reader->UpdateInformation();
  this->updateVariablesDialog();
  reader->GenerateObjectIdCellArrayOn();
  reader->GenerateGlobalElementIdArrayOn();
  reader->GenerateGlobalNodeIdArrayOn();
}

//----------------------------------------------------------------------------
const std::string& MooseViewer::getFileName(void)
{
  return this->FileName;
}

//----------------------------------------------------------------------------
void MooseViewer::setWidgetHintsFile(const std::string &whFile)
{
  this->widgetHintsFile = whFile;
}

//----------------------------------------------------------------------------
const std::string& MooseViewer::getWidgetHintsFile()
{
  return this->widgetHintsFile;
}

//----------------------------------------------------------------------------
GLMotif::PopupMenu* MooseViewer::createMainMenu(void)
{
  if (!this->widgetHints->isEnabled("MainMenu"))
    {
    std::cerr << "Ignoring hint to hide MainMenu." << std::endl;
    }
  this->widgetHints->pushGroup("MainMenu");

  GLMotif::PopupMenu* mainMenuPopup =
    new GLMotif::PopupMenu("MainMenuPopup",Vrui::getWidgetManager());
  mainMenuPopup->setTitle("Main Menu");
  GLMotif::Menu* mainMenu = new GLMotif::Menu("MainMenu",mainMenuPopup,false);

  if (this->widgetHints->isEnabled("Variables"))
    {
    GLMotif::ToggleButton *showVariablesDialog =
        new GLMotif::ToggleButton("ShowVariablesDialog", mainMenu, "Variables");
    showVariablesDialog->setToggle(false);
    showVariablesDialog->getValueChangedCallbacks().add(
          this, &MooseViewer::showVariableDialogCallback);
    }

  if (this->widgetHints->isEnabled("ColorBy"))
    {
    GLMotif::CascadeButton* colorByVariablesCascade =
        new GLMotif::CascadeButton("colorByVariablesCascade", mainMenu,
                                   "Color By");
    colorByVariablesCascade->setPopup(createColorByVariablesMenu());
    }

  if (this->widgetHints->isEnabled("ColorMap"))
    {
    GLMotif::CascadeButton * colorMapSubCascade =
        new GLMotif::CascadeButton("ColorMapSubCascade", mainMenu, "Color Map");
    colorMapSubCascade->setPopup(createColorMapSubMenu());
    }

  if (this->widgetHints->isEnabled("ColorEditor"))
    {
    GLMotif::ToggleButton * showColorEditorDialog =
        new GLMotif::ToggleButton("ShowColorEditorDialog", mainMenu,
                                  "Color Editor");
    showColorEditorDialog->setToggle(false);
    showColorEditorDialog->getValueChangedCallbacks().add(
          this, &MooseViewer::showColorEditorDialogCallback);
    }

  if (this->widgetHints->isEnabled("Animation"))
    {
    GLMotif::ToggleButton * showAnimationDialog =
        new GLMotif::ToggleButton("ShowAnimationDialog", mainMenu,
                                  "Animation");
    showAnimationDialog->setToggle(false);
    showAnimationDialog->getValueChangedCallbacks().add(
          this, &MooseViewer::showAnimationDialogCallback);
    }

  if (this->widgetHints->isEnabled("Representation"))
    {
    GLMotif::CascadeButton* representationCascade =
        new GLMotif::CascadeButton("RepresentationCascade", mainMenu,
                                   "Representation");
    representationCascade->setPopup(createRepresentationMenu());
    }

  if (this->widgetHints->isEnabled("AnalysisTools"))
    {
    GLMotif::CascadeButton* analysisToolsCascade =
        new GLMotif::CascadeButton("AnalysisToolsCascade", mainMenu,
                                   "Analysis Tools");
    analysisToolsCascade->setPopup(createAnalysisToolsMenu());
    }

  if (this->widgetHints->isEnabled("Contours"))
    {
    GLMotif::ToggleButton * showContoursDialog = new GLMotif::ToggleButton(
          "ShowContoursDialog", mainMenu, "Contours");
    showContoursDialog->setToggle(false);
    showContoursDialog->getValueChangedCallbacks().add(
          this, &MooseViewer::showContoursDialogCallback);
    }

  if (this->widgetHints->isEnabled("CenterDisplay"))
    {
    GLMotif::Button* centerDisplayButton =
        new GLMotif::Button("CenterDisplayButton",mainMenu,"Center Display");
    centerDisplayButton->getSelectCallbacks().add(
          this, &MooseViewer::centerDisplayCallback);
    }

  if (this->widgetHints->isEnabled("ToggleFPS"))
    {
    GLMotif::Button* toggleFPSButton =
        new GLMotif::Button("ToggleFPS",mainMenu,"Toggle FPS");
    toggleFPSButton->getSelectCallbacks().add(
          this, &MooseViewer::toggleFPSCallback);
    }

  if (this->widgetHints->isEnabled("Rendering"))
    {
    GLMotif::ToggleButton * showRenderingDialog =
        new GLMotif::ToggleButton("ShowRenderingDialog", mainMenu,
                                  "Rendering");
    showRenderingDialog->setToggle(false);
    showRenderingDialog->getValueChangedCallbacks().add(
          this, &MooseViewer::showRenderingDialogCallback);
    }

  mainMenu->manageChild();

  this->widgetHints->popGroup();

  return mainMenuPopup;
}

//----------------------------------------------------------------------------
GLMotif::Popup* MooseViewer::createRepresentationMenu(void)
{
  this->widgetHints->pushGroup("Representation");

  const GLMotif::StyleSheet* ss = Vrui::getWidgetManager()->getStyleSheet();

  GLMotif::Popup* representationMenuPopup =
    new GLMotif::Popup("representationMenuPopup", Vrui::getWidgetManager());
  GLMotif::SubMenu* representationMenu = new GLMotif::SubMenu(
    "representationMenu", representationMenuPopup, false);

  if (this->widgetHints->isEnabled("Outline"))
    {
    GLMotif::ToggleButton* showOutline=new GLMotif::ToggleButton(
          "ShowOutline",representationMenu,"Outline");
    showOutline->getValueChangedCallbacks().add(
          this,&MooseViewer::changeRepresentationCallback);
    showOutline->setToggle(true);
    }

  if (this->widgetHints->isEnabled("Representations"))
    {
    GLMotif::Label* representation_Label = new GLMotif::Label(
          "Representations", representationMenu,"Representations:");
    }

  GLMotif::RadioBox* representation_RadioBox =
    new GLMotif::RadioBox("Representation RadioBox",representationMenu,true);

  // Set to the default selected option. If left NULL, the first representation
  // is chosen.
  GLMotif::ToggleButton *selected = NULL;

  if (this->widgetHints->isEnabled("None"))
    {
    GLMotif::ToggleButton* showNone=new GLMotif::ToggleButton(
          "ShowNone",representation_RadioBox,"None");
    showNone->getValueChangedCallbacks().add(
          this,&MooseViewer::changeRepresentationCallback);
    }
  if (this->widgetHints->isEnabled("Points"))
    {
    GLMotif::ToggleButton* showPoints=new GLMotif::ToggleButton(
          "ShowPoints",representation_RadioBox,"Points");
    showPoints->getValueChangedCallbacks().add(
          this,&MooseViewer::changeRepresentationCallback);
    }
  if (this->widgetHints->isEnabled("Wireframe"))
    {
    GLMotif::ToggleButton* showWireframe=new GLMotif::ToggleButton(
          "ShowWireframe",representation_RadioBox,"Wireframe");
    showWireframe->getValueChangedCallbacks().add(
          this,&MooseViewer::changeRepresentationCallback);
    }
  if (this->widgetHints->isEnabled("Surface"))
    {
    GLMotif::ToggleButton* showSurface=new GLMotif::ToggleButton(
          "ShowSurface",representation_RadioBox,"Surface");
    showSurface->getValueChangedCallbacks().add(
          this,&MooseViewer::changeRepresentationCallback);
    selected = showSurface;
    }
  if (this->widgetHints->isEnabled("SurfaceWithEdges"))
    {
    GLMotif::ToggleButton* showSurfaceWithEdges=new GLMotif::ToggleButton(
          "ShowSurfaceWithEdges",representation_RadioBox,"Surface with Edges");
    showSurfaceWithEdges->getValueChangedCallbacks().add(
          this,&MooseViewer::changeRepresentationCallback);
    }
  if (this->widgetHints->isEnabled("Volume"))
    {
    GLMotif::ToggleButton* showVolume=new GLMotif::ToggleButton(
          "ShowVolume",representation_RadioBox,"Volume");
    showVolume->getValueChangedCallbacks().add(
          this,&MooseViewer::changeRepresentationCallback);
    }

  representation_RadioBox->setSelectionMode(GLMotif::RadioBox::ALWAYS_ONE);
  if (selected != NULL)
    {
    representation_RadioBox->setSelectedToggle(selected);
    }
  else
    {
    representation_RadioBox->setSelectedToggle(0);
    }

  representationMenu->manageChild();
  this->widgetHints->popGroup();
  return representationMenuPopup;
}

//----------------------------------------------------------------------------
GLMotif::Popup * MooseViewer::createAnalysisToolsMenu(void)
{
  this->widgetHints->pushGroup("AnalysisTools");

  const GLMotif::StyleSheet* ss = Vrui::getWidgetManager()->getStyleSheet();

  GLMotif::Popup * analysisToolsMenuPopup = new GLMotif::Popup(
    "analysisToolsMenuPopup", Vrui::getWidgetManager());
  GLMotif::SubMenu* analysisToolsMenu = new GLMotif::SubMenu(
    "representationMenu", analysisToolsMenuPopup, false);

  GLMotif::RadioBox * analysisTools_RadioBox = new GLMotif::RadioBox(
    "analysisTools", analysisToolsMenu, true);

  // Set to the default selected option. If left NULL, the first representation
  // is chosen.
  GLMotif::ToggleButton *selected = NULL;

  if (this->widgetHints->isEnabled("ClippingPlane"))
    {
    GLMotif::ToggleButton* showClippingPlane=new GLMotif::ToggleButton(
          "ClippingPlane",analysisTools_RadioBox,"Clipping Plane");
    showClippingPlane->getValueChangedCallbacks().add(
          this,&MooseViewer::changeAnalysisToolsCallback);
    selected = showClippingPlane;
    }

  analysisTools_RadioBox->setSelectionMode(GLMotif::RadioBox::ALWAYS_ONE);
  if (selected != NULL)
    {
    analysisTools_RadioBox->setSelectedToggle(selected);
    }
  else
    {
    analysisTools_RadioBox->setSelectedToggle(0);
    }

  analysisToolsMenu->manageChild();
  this->widgetHints->popGroup();
  return analysisToolsMenuPopup;
}

//----------------------------------------------------------------------------
GLMotif::Popup* MooseViewer::createColorByVariablesMenu(void)
{
  const GLMotif::StyleSheet* ss = Vrui::getWidgetManager()->getStyleSheet();

  GLMotif::Popup* colorByVariablesMenuPopup =
    new GLMotif::Popup("colorByVariablesMenuPopup", Vrui::getWidgetManager());
  this->colorByVariablesMenu = new GLMotif::SubMenu(
    "colorByVariablesMenu", colorByVariablesMenuPopup, false);

  this->colorByVariablesMenu->manageChild();
  return colorByVariablesMenuPopup;
}

//----------------------------------------------------------------------------
void MooseViewer::updateVariablesDialog(void)
{
  // Build sorted list of variables:
  std::set<std::string> vars;
  vars.insert(this->reader->GetPedigreeNodeIdArrayName());
  vars.insert(this->reader->GetObjectIdArrayName());
  vars.insert(this->reader->GetPedigreeElementIdArrayName());
  for (int i = 0; i < this->reader->GetNumberOfPointResultArrays(); ++i)
    {
    vars.insert(this->reader->GetPointResultArrayName(i));
    }
  for (int i = 0; i < this->reader->GetNumberOfElementResultArrays(); ++i)
    {
    vars.insert(this->reader->GetElementResultArrayName(i));
    }

  // Add to dialog:
  this->variablesDialog->clearAllVariables();
  typedef std::set<std::string>::const_iterator IterT;
  for (IterT it = vars.begin(), itEnd = vars.end(); it != itEnd; ++it)
    {
    this->variablesDialog->addVariable(*it);
    }
}

//----------------------------------------------------------------------------
std::string MooseViewer::getSelectedColorByArrayName(void) const
{
  std::string selectedToggle;
  GLMotif::RadioBox* radioBox =
    static_cast<GLMotif::RadioBox*> (colorByVariablesMenu->getChild(0));
  if (radioBox && (radioBox->getNumRows() > 0))
    {
    GLMotif::ToggleButton* selectedToggleButton =
      selectedToggleButton = radioBox->getSelectedToggle();
    if (selectedToggleButton)
      {
      selectedToggle.assign(selectedToggleButton->getString());
      }
    }
  return selectedToggle;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkDataArray> MooseViewer::getSelectedArray(int & type) const
{
  vtkSmartPointer<vtkDataArray> dataArray;
  std::string selectedArray = this->getSelectedColorByArrayName();
  if (selectedArray.empty())
    {
    return dataArray;
    }

  vtkSmartPointer<vtkCompositeDataGeometryFilter> compositeFilter =
    vtkSmartPointer<vtkCompositeDataGeometryFilter>::New();
  compositeFilter->SetInputConnection(this->reader->GetOutputPort());
  compositeFilter->Update();

  dataArray = vtkDataArray::SafeDownCast(
    compositeFilter->GetOutput()->GetPointData(
      )->GetArray(selectedArray.c_str()));
  if (!dataArray)
    {
    dataArray = vtkDataArray::SafeDownCast(
      compositeFilter->GetOutput()->GetCellData(
        )->GetArray(selectedArray.c_str()));
    if (!dataArray)
      {
      std::cerr << "The selected array is neither PointDataArray"\
        " nor CellDataArray" << std::endl;
      return dataArray;
      }
    else
      {
      type = 1; // CellData
      return dataArray;
      }
    }
  else
    {
    type = 0;
    return dataArray; // PointData
    }
}

//----------------------------------------------------------------------------
void MooseViewer::updateColorByVariablesMenu(void)
{
  /* Preserve the selection */
  std::string selectedToggle = this->getSelectedColorByArrayName();

  /* Clear the menu first */
  int i;
  for (i = this->colorByVariablesMenu->getNumRows(); i >= 0; --i)
    {
    colorByVariablesMenu->removeWidgets(i);
    }

  if (this->variables.size() > 0)
    {
    GLMotif::RadioBox* colorby_RadioBox =
      new GLMotif::RadioBox("Color RadioBox",colorByVariablesMenu,true);

    int selectedIndex = -1;

    typedef std::set<std::string>::const_iterator IterT;
    for (IterT it = this->variables.begin(), itEnd = this->variables.end();
         it != itEnd; ++it)
      {
      GLMotif::ToggleButton* button = new GLMotif::ToggleButton(
        it->c_str(), colorby_RadioBox, it->c_str());
      button->getValueChangedCallbacks().add(
        this, &MooseViewer::changeColorByVariablesCallback);
      button->setToggle(false);
      if (!selectedToggle.empty() && (selectedIndex < 0) &&
          (selectedToggle.compare(*it) == 0))
        {
        selectedIndex = std::distance(this->variables.begin(), it);
        }
      }

    selectedIndex = selectedIndex > 0 ? selectedIndex : 0;
    colorby_RadioBox->setSelectedToggle(selectedIndex);
    colorby_RadioBox->setSelectionMode(GLMotif::RadioBox::ALWAYS_ONE);
    this->updateScalarRange();
    this->updateHistogram();
    }
}

//----------------------------------------------------------------------------
GLMotif::Popup* MooseViewer::createColorMapSubMenu(void)
{
  this->widgetHints->pushGroup("ColorMap");

  GLMotif::Popup * colorMapSubMenuPopup = new GLMotif::Popup(
    "ColorMapSubMenuPopup", Vrui::getWidgetManager());
  GLMotif::RadioBox* colorMaps = new GLMotif::RadioBox(
    "ColorMaps", colorMapSubMenuPopup, false);
  colorMaps->setSelectionMode(GLMotif::RadioBox::ALWAYS_ONE);

  // This needs to end up pointing at InverseRainbow if enabled, or the first
  // map otherwise:
  int selectedToggle = 0;

  if (this->widgetHints->isEnabled("FullRainbow"))
    {
    ++selectedToggle;
    colorMaps->addToggle("Full Rainbow");
    }
  if (this->widgetHints->isEnabled("InverseFullRainbow"))
    {
    ++selectedToggle;
    colorMaps->addToggle("Inverse Full Rainbow");
    }
  if (this->widgetHints->isEnabled("Rainbow"))
    {
    ++selectedToggle;
    colorMaps->addToggle("Rainbow");
    }
  if (this->widgetHints->isEnabled("InverseRainbow"))
    {
    colorMaps->addToggle("Inverse Rainbow");
    }
  else
    {
    selectedToggle = 0;
    }
  if (this->widgetHints->isEnabled("ColdToHot"))
    {
    colorMaps->addToggle("Cold to Hot");
    }
  if (this->widgetHints->isEnabled("HotToCold"))
    {
    colorMaps->addToggle("Hot to Cold");
    }
  if (this->widgetHints->isEnabled("BlackToWhite"))
    {
    colorMaps->addToggle("Black to White");
    }
  if (this->widgetHints->isEnabled("WhiteToBlack"))
    {
    colorMaps->addToggle("White to Black");
    }
  if (this->widgetHints->isEnabled("HSBHues"))
    {
    colorMaps->addToggle("HSB Hues");
    }
  if (this->widgetHints->isEnabled("InverseHSBHues"))
    {
    colorMaps->addToggle("Inverse HSB Hues");
    }
  if (this->widgetHints->isEnabled("Davinci"))
    {
    colorMaps->addToggle("Davinci");
    }
  if (this->widgetHints->isEnabled("InverseDavinci"))
    {
    colorMaps->addToggle("Inverse Davinci");
    }
  if (this->widgetHints->isEnabled("Seismic"))
    {
    colorMaps->addToggle("Seismic");
    }
  if (this->widgetHints->isEnabled("InverseSeismic"))
    {
    colorMaps->addToggle("Inverse Seismic");
    }

  colorMaps->setSelectedToggle(selectedToggle);
  colorMaps->getValueChangedCallbacks().add(this,
    &MooseViewer::changeColorMapCallback);

  colorMaps->manageChild();

  this->widgetHints->popGroup();

  return colorMapSubMenuPopup;
} // end createColorMapSubMenu()

//----------------------------------------------------------------------------
GLMotif::PopupWindow* MooseViewer::createRenderingDialog(void) {
  const GLMotif::StyleSheet& ss = *Vrui::getWidgetManager()->getStyleSheet();
  GLMotif::PopupWindow * dialogPopup = new GLMotif::PopupWindow(
    "RenderingDialogPopup", Vrui::getWidgetManager(), "Rendering Dialog");
  GLMotif::RowColumn * dialog = new GLMotif::RowColumn(
    "RenderingDialog", dialogPopup, false);
  dialog->setOrientation(GLMotif::RowColumn::VERTICAL);
  dialog->setPacking(GLMotif::RowColumn::PACK_GRID);

  /* Create opacity slider */
  GLMotif::RowColumn * opacityRow = new GLMotif::RowColumn(
    "OpacityRow", dialog, false);
  opacityRow->setOrientation(GLMotif::RowColumn::HORIZONTAL);
  opacityRow->setPacking(GLMotif::RowColumn::PACK_GRID);
  GLMotif::Label* opacityLabel = new GLMotif::Label(
    "OpacityLabel", opacityRow, "Opacity");
  GLMotif::Slider* opacitySlider = new GLMotif::Slider(
    "OpacitySlider", opacityRow, GLMotif::Slider::HORIZONTAL,
    ss.fontHeight*10.0f);
  opacitySlider->setValue(Opacity);
  opacitySlider->setValueRange(0.0, 1.0, 0.1);
  opacitySlider->getValueChangedCallbacks().add(
    this, &MooseViewer::opacitySliderCallback);
  opacityValue = new GLMotif::TextField("OpacityValue", opacityRow, 6);
  opacityValue->setFieldWidth(6);
  opacityValue->setPrecision(3);
  opacityValue->setValue(Opacity);
  opacityRow->manageChild();

  /* Create Volume sampling options sliders */
  GLMotif::RowColumn * sampleRow = new GLMotif::RowColumn(
    "sampleRow", dialog, false);
  sampleRow->setOrientation(GLMotif::RowColumn::HORIZONTAL);
  sampleRow->setPacking(GLMotif::RowColumn::PACK_GRID);
  GLMotif::Label* sampleLabel = new GLMotif::Label(
    "SampleLabel", sampleRow, "Volume Sampling Dimensions");
  GLMotif::Slider* sampleSlider = new GLMotif::Slider(
    "SampleSlider", sampleRow, GLMotif::Slider::HORIZONTAL, ss.fontHeight*10.0f);
  sampleSlider->setValue(GaussianSplatterExp);
  sampleSlider->setValueRange(20, 200.0, 10.0);
  sampleSlider->getValueChangedCallbacks().add(
    this, &MooseViewer::sampleSliderCallback);
  sampleValue = new GLMotif::TextField("SampleValue", sampleRow, 6);
  sampleValue->setFieldWidth(6);
  sampleValue->setPrecision(3);
  std::stringstream stringstr;
  stringstr << GaussianSplatterDims << " x " << GaussianSplatterDims <<
    " x " << GaussianSplatterDims;
  sampleValue->setString(stringstr.str().c_str());
  sampleRow->manageChild();

  GLMotif::RowColumn * radiusRow = new GLMotif::RowColumn(
    "RadiusRow", dialog, false);
  radiusRow->setOrientation(GLMotif::RowColumn::HORIZONTAL);
  radiusRow->setPacking(GLMotif::RowColumn::PACK_GRID);
  GLMotif::Label* radiusLabel = new GLMotif::Label(
    "RadiusLabel", radiusRow, "Volume Sampling Radius");
  GLMotif::Slider* radiusSlider = new GLMotif::Slider(
    "RadiusSlider", radiusRow, GLMotif::Slider::HORIZONTAL,
    ss.fontHeight*10.0f);
  radiusSlider->setValue(GaussianSplatterRadius);
  radiusSlider->setValueRange(0.0, 0.1, 0.01);
  radiusSlider->getValueChangedCallbacks().add(
    this, &MooseViewer::radiusSliderCallback);
  radiusValue = new GLMotif::TextField("RadiusValue", radiusRow, 6);
  radiusValue->setFieldWidth(6);
  radiusValue->setPrecision(3);
  radiusValue->setValue(GaussianSplatterRadius);
  radiusRow->manageChild();

  GLMotif::RowColumn * exponentRow = new GLMotif::RowColumn(
    "ExponentRow", dialog, false);
  exponentRow->setOrientation(GLMotif::RowColumn::HORIZONTAL);
  exponentRow->setPacking(GLMotif::RowColumn::PACK_GRID);
  GLMotif::Label* expLabel = new GLMotif::Label(
    "ExpLabel", exponentRow, "Volume Sampling Exponent");
  GLMotif::Slider* exponentSlider = new GLMotif::Slider(
    "ExponentSlider", exponentRow, GLMotif::Slider::HORIZONTAL, ss.fontHeight*10.0f);
  exponentSlider->setValue(GaussianSplatterExp);
  exponentSlider->setValueRange(-5.0, 5.0, 1.0);
  exponentSlider->getValueChangedCallbacks().add(
    this, &MooseViewer::exponentSliderCallback);
  exponentValue = new GLMotif::TextField("ExponentValue", exponentRow, 6);
  exponentValue->setFieldWidth(6);
  exponentValue->setPrecision(3);
  exponentValue->setValue(GaussianSplatterExp);
  exponentRow->manageChild();

  dialog->manageChild();
  return dialogPopup;
}

//----------------------------------------------------------------------------
void MooseViewer::frame(void)
{
  this->FrameTimer.elapse();

  if(this->FirstFrame)
    {
    /* Initialize the color editor */
    this->ColorEditor = new TransferFunction1D(this);
    this->ColorEditor->createTransferFunction1D(CINVERSE_RAINBOW,
      UP_RAMP, 0.0, 1.0);
    this->ColorEditor->getColorMapChangedCallbacks().add(
      this, &MooseViewer::colorMapChangedCallback);
    this->ColorEditor->getAlphaChangedCallbacks().add(this,
      &MooseViewer::alphaChangedCallback);
    updateColorMap();
    updateAlpha();

    /* Contours */
    this->ContoursDialog = new Contours(this);
    this->ContoursDialog->getAlphaChangedCallbacks().add(this,
      &MooseViewer::contourValueChangedCallback);

    /* Initialize the Animation control */
    this->AnimationControl = new AnimationDialog(this);

    /* Compute the data center and Radius once */
    this->Center[0] = (this->DataBounds[0] + this->DataBounds[1])/2.0;
    this->Center[1] = (this->DataBounds[2] + this->DataBounds[3])/2.0;
    this->Center[2] = (this->DataBounds[4] + this->DataBounds[5])/2.0;

    this->Radius = sqrt((this->DataBounds[1] - this->DataBounds[0])*
                        (this->DataBounds[1] - this->DataBounds[0]) +
                        (this->DataBounds[3] - this->DataBounds[2])*
                        (this->DataBounds[3] - this->DataBounds[2]) +
                        (this->DataBounds[5] - this->DataBounds[4])*
                        (this->DataBounds[5] - this->DataBounds[4]));
    /* Scale the Radius */
    this->Radius *= 0.75;
    /* Initialize Vrui navigation transformation: */
    centerDisplayCallback(0);
    this->FirstFrame = false;
    }
  else
    {
    const size_t FPSCacheSize = 64;
    if (this->FrameTimes.size() < FPSCacheSize)
      {
      this->FrameTimes.push_back(this->FrameTimer.getTime());
      }
    else
      {
      std::rotate(this->FrameTimes.begin(), this->FrameTimes.begin() + 1,
                  this->FrameTimes.end());
      this->FrameTimes.back() = this->FrameTimer.getTime();
      }
    }

  // Get scalar array metadata.
  std::string arrayName = this->getSelectedColorByArrayName();
  ArrayLocator locator(this->reader->GetOutput(), arrayName);

  // Convert contour values from [0, 255] to scalar range:
  // TODO this would be nice to have handled already by the widget or callback.
  std::vector<double> scaledContourValues(this->ContourValues);
  for (std::vector<double>::iterator it = scaledContourValues.begin(),
       itEnd = scaledContourValues.end(); it != itEnd; ++it)
    {
    *it = (*it/255.) * (locator.Range[1] - locator.Range[0]) + locator.Range[0];
    }

  // Setup child GLObjects:
  this->m_contours->setVisible(this->ContourVisible);
  this->m_contours->setInput(reader->GetOutput());
  this->m_contours->setColorByArrayName(arrayName);
  this->m_contours->setContourValues(scaledContourValues);
  this->m_contours->update(locator);

  this->updateColorMap();
  this->updateAlpha();
  if (this->IsPlaying)
    {
    int currentTimeStep = this->reader->GetTimeStep();
    if (currentTimeStep < this->reader->GetTimeStepRange()[1])
      {
      this->reader->SetTimeStep(currentTimeStep + 1);
      Vrui::scheduleUpdate(Vrui::getApplicationTime() + 1.0/125.0);
      }
    else if(this->Loop)
      {
      this->reader->SetTimeStep(this->reader->GetTimeStepRange()[0]);
      Vrui::scheduleUpdate(Vrui::getApplicationTime() + 1.0/125.0);
      }
    else
      {
      this->AnimationControl->stopAnimation();
      this->IsPlaying = !this->IsPlaying;
      }
    }
  this->AnimationControl->updateTimeInformation();
  this->updateHistogram();
}

//----------------------------------------------------------------------------
void MooseViewer::initContext(GLContextData& contextData) const
{
  // The VTK OpenGL2 backend seems to require this:
  GLenum glewInitResult = glewInit();
  if (glewInitResult != GLEW_OK)
    {
    std::cerr << "Error: Could not initialize GLEW (glewInit() returned: "
      << glewInitResult << ")." << std::endl;
    }

  /* Create a new context data item */
  DataItem* dataItem = new DataItem();
  contextData.addDataItem(this, dataItem);

  m_contours->initRenderer(contextData, dataItem->renderer);
  m_contours->setLookupTable(contextData, dataItem->lut);

  vtkNew<vtkOutlineFilter> dataOutline;

  dataItem->compositeFilter->SetInputConnection(this->reader->GetOutputPort());
  dataItem->compositeFilter->Update();
  dataItem->compositeFilter->GetOutput()->GetBounds(this->DataBounds);

  dataOutline->SetInputConnection(dataItem->compositeFilter->GetOutputPort());

  dataItem->mapperVolume->SetRequestedRenderMode(this->RequestedRenderMode);

  vtkNew<vtkPolyDataMapper> mapperOutline;
  mapperOutline->SetInputConnection(dataOutline->GetOutputPort());
  dataItem->actorOutline->SetMapper(mapperOutline.GetPointer());
  dataItem->actorOutline->GetProperty()->SetColor(1,1,1);
}

//----------------------------------------------------------------------------
void MooseViewer::display(GLContextData& contextData) const
{
  int numberOfSupportedClippingPlanes;
  glGetIntegerv(GL_MAX_CLIP_PLANES, &numberOfSupportedClippingPlanes);
  int clippingPlaneIndex = 0;
  for (int i = 0; i < NumberOfClippingPlanes &&
    clippingPlaneIndex < numberOfSupportedClippingPlanes; ++i)
    {
    if (ClippingPlanes[i].isActive())
      {
        /* Enable the clipping plane: */
        glEnable(GL_CLIP_PLANE0 + clippingPlaneIndex);
        GLdouble clippingPlane[4];
        for (int j = 0; j < 3; ++j)
            clippingPlane[j] = ClippingPlanes[i].getPlane().getNormal()[j];
        clippingPlane[3] = -ClippingPlanes[i].getPlane().getOffset();
        glClipPlane(GL_CLIP_PLANE0 + clippingPlaneIndex, clippingPlane);
        /* Go to the next clipping plane: */
        ++clippingPlaneIndex;
      }
    }

  /* Grab the reader's data set. */
  vtkDataSet *inputDataSet =
      vtkDataSet::SafeDownCast(
        vtkMultiBlockDataSet::SafeDownCast(
          this->reader->GetOutput()->GetBlock(0))->GetBlock(0));

  /* Get context data item */
  DataItem* dataItem = contextData.retrieveDataItem<DataItem>(this);

  // Update framerate:
  if (this->ShowFPS)
    {
    std::ostringstream fps;
    fps << "FPS: " << this->GetFramesPerSecond();
    dataItem->framerate->SetInput(fps.str().c_str());
    dataItem->framerate->SetVisibility(1);
    }
  else
    {
    dataItem->framerate->SetVisibility(0);
    }

  /* Color by selected array */
  std::string selectedArray = this->getSelectedColorByArrayName();
  int selectedArrayType = -1;
  double* dataRange = NULL;
  if (!selectedArray.empty())
    {
    // TODO This is screwing up the mtimes. This should be set on the filters
    // that need it.
//    inputDataSet->GetPointData()->SetActiveScalars(selectedArray.c_str());

    dataItem->mapper->SelectColorArray(selectedArray.c_str());

    bool dataArrayFound = true;

    vtkSmartPointer<vtkUnstructuredGrid> usg =
      vtkSmartPointer<vtkUnstructuredGrid>::New();
    usg->DeepCopy(
      vtkUnstructuredGrid::SafeDownCast(vtkMultiBlockDataSet::SafeDownCast(
          this->reader->GetOutput()->GetBlock(0))->GetBlock(0)));

    dataItem->compositeFilter->Update();
    vtkSmartPointer<vtkDataArray> dataArray = vtkDataArray::SafeDownCast(
      dataItem->compositeFilter->GetOutput()->GetPointData(
        )->GetArray(selectedArray.c_str()));
    if (!dataArray)
      {
      dataArray = vtkDataArray::SafeDownCast(
        dataItem->compositeFilter->GetOutput()->GetCellData(
          )->GetArray(selectedArray.c_str()));
      dataItem->mapper->SetScalarModeToUseCellFieldData();
      vtkSmartPointer<vtkCellDataToPointData> cellToPoint =
        vtkSmartPointer<vtkCellDataToPointData>::New();
      cellToPoint->SetInputData(usg);
      cellToPoint->Update();
      usg->DeepCopy(vtkUnstructuredGrid::SafeDownCast(cellToPoint->GetOutput()));
      if (!dataArray)
        {
        std::cerr << "The selected array is neither PointDataArray"\
          " nor CellDataArray" << std::endl;
        dataArrayFound = false;
        }
      else
        {
        selectedArrayType = 1;
        }
      }
    else
      {
      dataItem->mapper->SetScalarModeToUsePointFieldData();
      selectedArrayType = 0;
      }

    if (dataArrayFound)
      {
      dataRange = dataArray->GetRange();
      dataRange[0] = this->ScalarRange[0];
      dataRange[1] = this->ScalarRange[1];
      dataItem->mapper->SetScalarRange(dataRange);
      dataItem->lut->SetTableRange(dataRange);
      }

    usg->GetPointData()->SetActiveScalars(selectedArray.c_str());
    int imageExtent[6] = {0, this->GaussianSplatterDims-1,
      0, this->GaussianSplatterDims-1, 0, this->GaussianSplatterDims-1};
    dataItem->gaussian->GetOutput()->SetExtent(imageExtent);
    dataItem->gaussian->SetInputData(usg);
    dataItem->gaussian->SetModelBounds(usg->GetBounds());
    dataItem->gaussian->SetSampleDimensions(this->GaussianSplatterDims,
      this->GaussianSplatterDims, this->GaussianSplatterDims);
    dataItem->gaussian->SetRadius(this->GaussianSplatterRadius*10);
    dataItem->gaussian->SetExponentFactor(this->GaussianSplatterExp);

    dataItem->colorFunction->RemoveAllPoints();
    dataItem->opacityFunction->RemoveAllPoints();
    double dataRangeMax = dataRange ? dataRange[1] : 1.0;
    double dataRangeMin = dataRange ? dataRange[0] : 0.0;
    double step = (dataRangeMax - dataRangeMin)/255.0;
    for (int i = 0; i < 256; ++i)
      {
      dataItem->lut->SetTableValue(i,
        this->ColorMap[4*i + 0],
        this->ColorMap[4*i + 1],
        this->ColorMap[4*i + 2],
        this->ColorMap[4*i + 3]);
      dataItem->colorFunction->AddRGBPoint(
        dataRangeMin + (double)(i*step),
        this->ColorMap[4*i + 0],
        this->ColorMap[4*i + 1],
        this->ColorMap[4*i + 2]);
      dataItem->opacityFunction->AddPoint(
        dataRangeMin + (double)(i*step),
        this->ColorMap[4*i + 3]);
      }
    }

  /* Enable/disable the outline */
  if (this->Outline)
    {
    dataItem->actorOutline->VisibilityOn();
    }
  else
    {
    dataItem->actorOutline->VisibilityOff();
    }

  /* Set actor opacity */
  dataItem->actor->GetProperty()->SetOpacity(this->Opacity);
  /* Set the appropriate representation */
  if (this->RepresentationType != -1)
    {
    dataItem->actor->VisibilityOn();
    if (this->RepresentationType == 3)
      {
      dataItem->actor->GetProperty()->SetRepresentationToSurface();
      dataItem->actor->GetProperty()->EdgeVisibilityOn();
      }
    else
      {
      dataItem->actor->GetProperty()->SetRepresentation(
        this->RepresentationType);
      dataItem->actor->GetProperty()->EdgeVisibilityOff();
      }
    }
  else
    {
    dataItem->actor->VisibilityOff();
    }
  if (this->Volume)
    {
    if (!selectedArray.empty())
      {
      dataItem->actorVolume->VisibilityOn();
      }
    }
  else
    {
    dataItem->actorVolume->VisibilityOff();
    }

  /* Contours */
  m_contours->syncContext(contextData);

  /* Render the scene */
  dataItem->externalVTKWidget->GetRenderWindow()->Render();

  clippingPlaneIndex = 0;
  for (int i = 0; i < NumberOfClippingPlanes &&
    clippingPlaneIndex < numberOfSupportedClippingPlanes; ++i)
    {
    if (ClippingPlanes[i].isActive())
      {
        /* Disable the clipping plane: */
        glDisable(GL_CLIP_PLANE0 + clippingPlaneIndex);
        /* Go to the next clipping plane: */
        ++clippingPlaneIndex;
      }
    }
}

//----------------------------------------------------------------------------
void MooseViewer::toggleFPSCallback(Misc::CallbackData *cbData)
{
  this->ShowFPS = !this->ShowFPS;
}

//----------------------------------------------------------------------------
void MooseViewer::centerDisplayCallback(Misc::CallbackData* callBackData)
{
  if(!this->DataBounds)
    {
    std::cerr << "ERROR: Data bounds not set!!" << std::endl;
    return;
    }
  Vrui::setNavigationTransformation(this->Center, this->Radius);
}

//----------------------------------------------------------------------------
void MooseViewer::opacitySliderCallback(
  GLMotif::Slider::ValueChangedCallbackData* callBackData)
{
  this->Opacity = static_cast<double>(callBackData->value);
  opacityValue->setValue(callBackData->value);
}

//----------------------------------------------------------------------------
void MooseViewer::sampleSliderCallback(
  GLMotif::Slider::ValueChangedCallbackData* callBackData)
{
  this->GaussianSplatterDims = static_cast<double>(callBackData->value);
  std::stringstream ss;
  ss << GaussianSplatterDims << " x " << GaussianSplatterDims <<
    " x " << GaussianSplatterDims;
  sampleValue->setString(ss.str().c_str());
}

//----------------------------------------------------------------------------
void MooseViewer::radiusSliderCallback(
  GLMotif::Slider::ValueChangedCallbackData* callBackData)
{
  this->GaussianSplatterRadius = static_cast<double>(callBackData->value);
  radiusValue->setValue(callBackData->value);
}

//----------------------------------------------------------------------------
void MooseViewer::exponentSliderCallback(
  GLMotif::Slider::ValueChangedCallbackData* callBackData)
{
  this->GaussianSplatterExp = static_cast<double>(callBackData->value);
  exponentValue->setValue(callBackData->value);
}

//----------------------------------------------------------------------------
void MooseViewer::showVariableDialogCallback(
    GLMotif::ToggleButton::ValueChangedCallbackData *callBackData)
{
  GLMotif::WidgetManager *mgr = Vrui::getWidgetManager();

  if (callBackData->set)
    {
    GLMotif::WidgetManager::Transformation xform =
        mgr->calcWidgetTransformation(mainMenu);
    mgr->popupPrimaryWidget(this->variablesDialog, xform);
    }
  else
    {
    mgr->popdownWidget(this->variablesDialog);
    }
}

//----------------------------------------------------------------------------
void MooseViewer::changeRepresentationCallback(
  GLMotif::ToggleButton::ValueChangedCallbackData* callBackData)
{
  /* Adjust representation state based on which toggle button changed state: */
  if (strcmp(callBackData->toggle->getName(), "ShowSurface") == 0)
    {
    this->RepresentationType = 2;
    this->Volume = false;
    }
  else if (strcmp(callBackData->toggle->getName(), "ShowSurfaceWithEdges") == 0)
    {
    this->RepresentationType = 3;
    this->Volume = false;
    }
  else if (strcmp(callBackData->toggle->getName(), "ShowWireframe") == 0)
    {
    this->RepresentationType = 1;
    this->Volume = false;
    }
  else if (strcmp(callBackData->toggle->getName(), "ShowPoints") == 0)
    {
    this->RepresentationType = 0;
    this->Volume = false;
    }
  else if (strcmp(callBackData->toggle->getName(), "ShowNone") == 0)
    {
    this->RepresentationType = -1;
    this->Volume = false;
    }
  else if (strcmp(callBackData->toggle->getName(), "ShowVolume") == 0)
    {
    this->Volume = callBackData->set;
    if (this->Volume)
      {
      this->RepresentationType = -1;
      }
    }
  else if (strcmp(callBackData->toggle->getName(), "ShowOutline") == 0)
    {
    this->Outline = callBackData->set;
    }
}
//----------------------------------------------------------------------------
void MooseViewer::changeAnalysisToolsCallback(
  GLMotif::ToggleButton::ValueChangedCallbackData* callBackData)
{
  /* Set the new analysis tool: */
  if (strcmp(callBackData->toggle->getName(), "ClippingPlane") == 0)
  {
    this->analysisTool = 0;
  }
}

//----------------------------------------------------------------------------
void MooseViewer::showRenderingDialogCallback(
  GLMotif::ToggleButton::ValueChangedCallbackData* callBackData)
{
  /* open/close rendering dialog based on which toggle button changed state: */
  if (strcmp(callBackData->toggle->getName(), "ShowRenderingDialog") == 0)
    {
    if (callBackData->set)
      {
      /* Open the rendering dialog at the same position as the main menu: */
      Vrui::getWidgetManager()->popupPrimaryWidget(
        renderingDialog,
        Vrui::getWidgetManager()->calcWidgetTransformation(mainMenu));
      }
    else
      {
      /* Close the rendering dialog: */
      Vrui::popdownPrimaryWidget(renderingDialog);
      }
    }
}

//----------------------------------------------------------------------------
void MooseViewer::showColorEditorDialogCallback(
  GLMotif::ToggleButton::ValueChangedCallbackData* callBackData)
{
  /* open/close transfer function dialog based on which toggle button changed state: */
  if (strcmp(callBackData->toggle->getName(), "ShowColorEditorDialog") == 0)
    {
    if (callBackData->set)
      {
      /* Open the transfer function dialog at the same position as the main menu: */
      Vrui::getWidgetManager()->popupPrimaryWidget(this->ColorEditor,
        Vrui::getWidgetManager()->calcWidgetTransformation(mainMenu));
      }
    else
      {
      /* Close the transfer function dialog: */
      Vrui::popdownPrimaryWidget(this->ColorEditor);
    }
  }
}

//----------------------------------------------------------------------------
void MooseViewer::showAnimationDialogCallback(
  GLMotif::ToggleButton::ValueChangedCallbackData* callBackData)
{
  /* open/close animation dialog based on which toggle button changed state: */
  if (strcmp(callBackData->toggle->getName(), "ShowAnimationDialog") == 0)
    {
    if (callBackData->set)
      {
      /* Open the animation dialog at the same position as the main menu: */
      Vrui::getWidgetManager()->popupPrimaryWidget(this->AnimationControl,
        Vrui::getWidgetManager()->calcWidgetTransformation(mainMenu));
      }
    else
      {
      /* Close the transfer function dialog: */
      Vrui::popdownPrimaryWidget(this->AnimationControl);
    }
  }
}

//----------------------------------------------------------------------------
ClippingPlane * MooseViewer::getClippingPlanes(void)
{
  return this->ClippingPlanes;
}

//----------------------------------------------------------------------------
int MooseViewer::getNumberOfClippingPlanes(void)
{
  return this->NumberOfClippingPlanes;
}

//----------------------------------------------------------------------------
void MooseViewer::toolCreationCallback(
  Vrui::ToolManager::ToolCreationCallbackData * callbackData)
{
  /* Check if the new tool is a locator tool: */
  Vrui::LocatorTool* locatorTool =
    dynamic_cast<Vrui::LocatorTool*> (callbackData->tool);
  if (locatorTool != 0)
    {
    BaseLocator* newLocator;
    if (analysisTool == 0)
      {
      /* Create a clipping plane locator object and
       * associate it with the new tool:
       */
      newLocator = new ClippingPlaneLocator(locatorTool, this);
      }

    /* Add new locator to list: */
    baseLocators.push_back(newLocator);
    }
}

//----------------------------------------------------------------------------
void MooseViewer::toolDestructionCallback(
  Vrui::ToolManager::ToolDestructionCallbackData * callbackData)
{
  /* Check if the to-be-destroyed tool is a locator tool: */
  Vrui::LocatorTool* locatorTool =
    dynamic_cast<Vrui::LocatorTool*> (callbackData->tool);
  if (locatorTool != 0)
    {
    /* Find the data locator associated with the tool in the list: */
    for (BaseLocatorList::iterator blIt = baseLocators.begin();
      blIt != baseLocators.end(); ++blIt)
      {
      if ((*blIt)->getTool() == locatorTool)
        {
        /* Remove the locator: */
        delete *blIt;
        baseLocators.erase(blIt);
        break;
        }
      }
    }
}

//----------------------------------------------------------------------------
void MooseViewer::changeVariablesCallback(
    GLMotif::ListBox::SelectionChangedCallbackData *callBackData)
{
  bool enable;
  switch (callBackData->reason)
    {
    case GLMotif::ListBox::SelectionChangedCallbackData::SELECTION_CLEARED:
      // Nothing should call selectionCleared, handle this if that changes:
      std::cerr << "Unhandled SELECTION_CLEARED event." << std::endl;
      return;
    default:
      std::cerr << "Unrecognized variable change reason: "
                << callBackData->reason << std::endl;
      return;
    case GLMotif::ListBox::SelectionChangedCallbackData::NUMITEMS_CHANGED:
      return; // don't care
    case GLMotif::ListBox::SelectionChangedCallbackData::ITEM_SELECTED:
      enable = true;
      break;
    case GLMotif::ListBox::SelectionChangedCallbackData::ITEM_DESELECTED:
      enable = false;
      break;
    }

  std::string array(callBackData->listBox->getItem(callBackData->item));

  int numPointResultArrays = this->reader->GetNumberOfPointResultArrays();
  int numElementResultArrays = this->reader->GetNumberOfElementResultArrays();

  int i;
  for (i = 0; i < numPointResultArrays; ++i)
    {
    if (array == this->reader->GetPointResultArrayName(i))
      {
      this->reader->SetPointResultArrayStatus(array.c_str(), enable ? 1 : 0);
      break;
      }
    }
  if (i == numPointResultArrays)
    {
    for (i = 0; i < numElementResultArrays; ++i)
      {
      if (array == this->reader->GetElementResultArrayName(i))
        {
        this->reader->SetElementResultArrayStatus(array.c_str(),
                                                  enable ? 1 : 0);
        break;
        }
      }
    }

  if (enable)
    {
    this->variables.insert(array);
    }
  else
    {
    this->variables.erase(array);
    }

  this->updateColorByVariablesMenu();
}

//----------------------------------------------------------------------------
void MooseViewer::changeColorByVariablesCallback(
  GLMotif::ToggleButton::ValueChangedCallbackData* callBackData)
{
  if (this->variables.size() == 1)
    {
    callBackData->toggle->setToggle(true);
    }
  this->updateHistogram();
  this->updateScalarRange();
  Vrui::requestUpdate();
}

//----------------------------------------------------------------------------
void MooseViewer::changeColorMapCallback(
  GLMotif::RadioBox::ValueChangedCallbackData* callBackData)
{
  int value = callBackData->radioBox->getToggleIndex(
    callBackData->newSelectedToggle);
  this->ColorEditor->changeColorMap(value);
  this->updateColorMap();
  this->updateAlpha();
  Vrui::requestUpdate();
}

//----------------------------------------------------------------------------
void MooseViewer::colorMapChangedCallback(
  Misc::CallbackData* callBackData)
{
  this->ColorEditor->exportColorMap(this->ColorMap);
  this->updateAlpha();
  Vrui::requestUpdate();
}

//----------------------------------------------------------------------------
void MooseViewer::alphaChangedCallback(Misc::CallbackData* callBackData)
{
  this->ColorEditor->exportAlpha(this->ColorMap);
  Vrui::requestUpdate();
}

//----------------------------------------------------------------------------
void MooseViewer::updateAlpha(void)
{
  this->ColorEditor->exportAlpha(this->ColorMap);
  Vrui::requestUpdate();
}

//----------------------------------------------------------------------------
void MooseViewer::updateColorMap(void)
{
  this->ColorEditor->exportColorMap(this->ColorMap);
  Vrui::requestUpdate();
}

//----------------------------------------------------------------------------
float * MooseViewer::getHistogram(void)
{
  return this->Histogram;
}

//----------------------------------------------------------------------------
void MooseViewer::updateHistogram(void)
{
  /* Clear the histogram */
  for(int j = 0; j < 256; ++j)
    {
    this->Histogram[j] = 0.0;
    }

  int type = -1;
  vtkSmartPointer<vtkDataArray> dataArray = this->getSelectedArray(type);
  if (!dataArray || (type < 0))
    {
    return;
    }

  double * scalarRange = dataArray->GetRange();
  if (fabs(scalarRange[1] - scalarRange[0]) < 1e-6)
    {
    return;
    }

  // Divide the range into 256 bins
  for (int i = 0; i < dataArray->GetNumberOfTuples(); ++i)
    {
    double * tuple = dataArray->GetTuple(i);
    int bin = static_cast<int>((tuple[0] - scalarRange[0])*255.0 /
      (scalarRange[1] - scalarRange[0]));
    this->Histogram[bin] += 1;
    }

  this->ColorEditor->setHistogram(this->Histogram);
  this->ContoursDialog->setHistogram(this->Histogram);
  Vrui::requestUpdate();
}

//----------------------------------------------------------------------------
double MooseViewer::GetFramesPerSecond() const
{
  double time = 0.0;
  for (size_t i = 0; i < this->FrameTimes.size(); ++i)
    {
    time += this->FrameTimes[i];
    }
  return time > 1e-5 ? this->FrameTimes.size() / time : 0.;
}

//----------------------------------------------------------------------------
void MooseViewer::updateScalarRange(void)
{
  int type = -1;
  vtkSmartPointer<vtkDataArray> dataArray = this->getSelectedArray(type);
  if (!dataArray || (type < 0))
    {
    return;
    }

  double * scalarRange = dataArray->GetRange();
  if (fabs(scalarRange[1] - scalarRange[0]) < 1e-6)
    {
    return;
    }

  this->ScalarRange[0] = scalarRange[0];
  this->ScalarRange[1] = scalarRange[1];
  this->ColorEditor->setScalarRange(scalarRange);
}

//----------------------------------------------------------------------------
void MooseViewer::showContoursDialogCallback(
  GLMotif::ToggleButton::ValueChangedCallbackData* callBackData)
{
  /* open/close slices dialog based on which toggle button changed state: */
  if (strcmp(callBackData->toggle->getName(), "ShowContoursDialog") == 0)
    {
    if (callBackData->set)
      {
      /* Open the slices dialog at the same position as the main menu: */
      Vrui::getWidgetManager()->popupPrimaryWidget(ContoursDialog,
        Vrui::getWidgetManager()->calcWidgetTransformation(mainMenu));
      }
    else
      {
      /* Close the slices dialog: */
      Vrui::popdownPrimaryWidget(ContoursDialog);
      }
    }
}

//----------------------------------------------------------------------------
void MooseViewer::contourValueChangedCallback(Misc::CallbackData* callBackData)
{
  this->ContourValues = ContoursDialog->getContourValues();
  Vrui::requestUpdate();
}

//----------------------------------------------------------------------------
void MooseViewer::setContourVisible(bool visible)
{
  this->ContourVisible = visible;
}

//----------------------------------------------------------------------------
void MooseViewer::setRequestedRenderMode(int mode)
{
  this->RequestedRenderMode = mode;
}

//----------------------------------------------------------------------------
int MooseViewer::getRequestedRenderMode(void) const
{
  return this->RequestedRenderMode;
}

//----------------------------------------------------------------------------
void MooseViewer::setScalarMinimum(double min)
{
  if (min < this->ScalarRange[1])
    {
    this->ScalarRange[0] = min;
    }
  Vrui::requestUpdate();
}

//----------------------------------------------------------------------------
void MooseViewer::setScalarMaximum(double max)
{
  if (max > this->ScalarRange[0])
    {
    this->ScalarRange[1] = max;
    }
  Vrui::requestUpdate();
}
