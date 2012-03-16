/*
Copyright (C) 2010 David Doria, daviddoria@gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "GraphCutSegmentationWidget.h"

#include "Helpers.h"

// ITK
#include <itkCastImageFilter.h>
#include <itkCovariantVector.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkImageRegionConstIteratorWithIndex.h>
#include <itkLineIterator.h>
#include <itkNthElementImageAdaptor.h>

// VTK
#include <vtkCamera.h>
#include <vtkImageActor.h>
#include <vtkImageData.h>
#include <vtkInteractorStyleImage.h>
#include <vtkPolyData.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkSmartPointer.h>

// Qt
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QtConcurrentRun>

// STL
#include <iostream>

GraphCutSegmentationWidget::GraphCutSegmentationWidget(const std::string& fileName) : QMainWindow(NULL)
{
  SharedConstructor();
  OpenFile(fileName);
}

GraphCutSegmentationWidget::GraphCutSegmentationWidget(QWidget *parent) : QMainWindow(parent)
{
  SharedConstructor();
}

void GraphCutSegmentationWidget::SharedConstructor()
{
  // Setup the GUI and connect all of the signals and slots
  setupUi(this);

  this->ProgressDialog = new QProgressDialog();
  this->ProgressDialog->setMinimum(0);
  this->ProgressDialog->setMaximum(0);
  this->ProgressDialog->setWindowModality(Qt::WindowModal);
  connect(&this->FutureWatcher, SIGNAL(finished()), this, SLOT(slot_SegmentationComplete()));
  connect(&this->FutureWatcher, SIGNAL(finished()), this->ProgressDialog , SLOT(cancel()));
  
  connect( this->sldHistogramBins, SIGNAL( valueChanged(int) ), this, SLOT(sldHistogramBins_valueChanged()));
  connect( this->sldLambda, SIGNAL( valueChanged(int) ), this, SLOT(UpdateLambda()));
  connect( this->txtLambdaMax, SIGNAL( textEdited(QString) ), this, SLOT(UpdateLambda()));

  this->BackgroundColor[0] = 0;
  this->BackgroundColor[1] = 0;
  this->BackgroundColor[2] = .5;

  this->CameraUp[0] = 0;
  this->CameraUp[1] = 1;
  this->CameraUp[2] = 0;

  // Instantiations
  this->OriginalImageActor = vtkSmartPointer<vtkImageActor>::New();
  this->ResultActor = vtkSmartPointer<vtkImageActor>::New();

  // Add renderers - we flip the image by changing the camera view up because of the conflicting
  // conventions used by ITK and VTK
  this->LeftRenderer = vtkSmartPointer<vtkRenderer>::New();
  this->LeftRenderer->GradientBackgroundOn();
  this->LeftRenderer->SetBackground(this->BackgroundColor);
  this->LeftRenderer->SetBackground2(1,1,1);
  this->LeftRenderer->GetActiveCamera()->SetViewUp(this->CameraUp);
  this->qvtkWidgetLeft->GetRenderWindow()->AddRenderer(this->LeftRenderer);

  this->RightRenderer = vtkSmartPointer<vtkRenderer>::New();
  this->RightRenderer->GradientBackgroundOn();
  this->RightRenderer->SetBackground(this->BackgroundColor);
  this->RightRenderer->SetBackground2(1,1,1);
  this->RightRenderer->GetActiveCamera()->SetViewUp(this->CameraUp);
  this->qvtkWidgetRight->GetRenderWindow()->AddRenderer(this->RightRenderer);

  // Setup right interactor style
  vtkSmartPointer<vtkInteractorStyleImage> interactorStyleImage =
    vtkSmartPointer<vtkInteractorStyleImage>::New();
  this->qvtkWidgetRight->GetInteractor()->SetInteractorStyle(interactorStyleImage);

  // Setup left interactor style
  this->GraphCutStyle = vtkSmartPointer<vtkScribbleInteractorStyle>::New();
  this->qvtkWidgetLeft->GetInteractor()->SetInteractorStyle(this->GraphCutStyle);

  // Default GUI settings
  this->radForeground->setChecked(true);

  // Setup toolbar
  // Open file buttons
  QIcon openIcon = QIcon::fromTheme("document-open");
  actionOpenImage->setIcon(openIcon);
  this->toolBar->addAction(actionOpenImage);

  // Save buttons
  QIcon saveIcon = QIcon::fromTheme("document-save");
  actionSaveSegmentation->setIcon(saveIcon);
  this->toolBar->addAction(actionSaveSegmentation);
}
void GraphCutSegmentationWidget::on_actionExit_triggered()
{
  exit(0);
}

void GraphCutSegmentationWidget::on_actionFlipImageVertically_triggered()
{
  this->CameraUp[1] *= -1;
  this->LeftRenderer->GetActiveCamera()->SetViewUp(this->CameraUp);
  this->RightRenderer->GetActiveCamera()->SetViewUp(this->CameraUp);
  this->Refresh();
}

void GraphCutSegmentationWidget::on_actionFlipImageHorizontally_triggered()
{
  this->CameraUp[0] *= -1;
  this->LeftRenderer->GetActiveCamera()->SetViewUp(this->CameraUp);
  this->RightRenderer->GetActiveCamera()->SetViewUp(this->CameraUp);
  this->Refresh();
}

void GraphCutSegmentationWidget::on_actionSaveSegmentation_triggered()
{
  // Ask the user for a filename to save the segment mask image to

  QString fileName = QFileDialog::getSaveFileName(this,
    "Save Segment Mask Image", ".", "Image Files (*.png *.mha)");
/*
  // Convert the image from a 1D vector image to an unsigned char image
  typedef itk::CastImageFilter< GrayscaleImageType, itk::Image<itk::CovariantVector<unsigned char, 1>, 2 > > CastFilterType;
  CastFilterType::Pointer castFilter = CastFilterType::New();
  castFilter->SetInput(this->GraphCut->GetSegmentMask());

  typedef itk::NthElementImageAdaptor< itk::Image<itk:: CovariantVector<unsigned char, 1>, 2 >,
    unsigned char> ImageAdaptorType;

  ImageAdaptorType::Pointer adaptor = ImageAdaptorType::New();
  adaptor->SelectNthElement(0);
  adaptor->SetImage(castFilter->GetOutput());
*/
/*
  // Write the file (object is white)
  //typedef  itk::ImageFileWriter< ImageAdaptorType > WriterType;
  typedef  itk::ImageFileWriter< MaskImageType > WriterType;
  WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(fileName.toStdString());
  //writer->SetInput(adaptor);
  writer->SetInput(this->GraphCut.GetSegmentMask());
  writer->Update();
  */

  // Write the inverted file (object is black)
  MaskImageType::Pointer inverted = MaskImageType::New();
  Helpers::InvertBinaryImage(this->GraphCut.GetSegmentMask(), inverted);
  
  //typedef  itk::ImageFileWriter< ImageAdaptorType > WriterType;
  typedef  itk::ImageFileWriter< MaskImageType > WriterType;
  WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(fileName.toStdString());
  writer->SetInput(inverted);
  writer->Update();

}

void GraphCutSegmentationWidget::on_actionOpenImage_triggered()
{
  //std::cout << "actionOpenImage_triggered()" << std::endl;
    //std::cout << "Enter OpenFile()" << std::endl;

  // Get a filename to open
  QString filename = QFileDialog::getOpenFileName(this,
     "Open Image", ".", "Image Files (*.png *.mha)");

  if(filename.isEmpty())
    {
    return;
    }
  OpenFile(filename.toStdString());
}

/*
 // Display segmented image with black background pixels
void Form::StopProgressSlot()
{
  // When the ProgressThread emits the StopProgressSignal, we need to display the result of the segmentation

  // Convert the masked image into a VTK image for display
  vtkSmartPointer<vtkImageData> VTKSegmentMask =
    vtkSmartPointer<vtkImageData>::New();
  if(this->GraphCut->GetPixelDimensionality() == 1)
    {
    ITKImagetoVTKImage<GrayscaleImageType>(static_cast<ImageGraphCut<GrayscaleImageType>* >(this->GraphCut)->GetMaskedOutput(), VTKSegmentMask);
    }
  else if(this->GraphCut->GetPixelDimensionality() == 3)
    {
    ITKImagetoVTKImage<ColorImageType>(static_cast<ImageGraphCut<ColorImageType>* >(this->GraphCut)->GetMaskedOutput(), VTKSegmentMask);
    }
  else if(this->GraphCut->GetPixelDimensionality() == 5)
    {
    ITKImagetoVTKImage<RGBDIImageType>(static_cast<ImageGraphCut<RGBDIImageType>* >(this->GraphCut)->GetMaskedOutput(), VTKSegmentMask);
    }
  else
    {
    std::cerr << "This type of image (" << this->GraphCut->GetPixelDimensionality() << ") cannot be displayed!" << std::endl;
    exit(-1);
    }

  // Remove the old output, set the new output and refresh everything
  this->ResultActor = vtkSmartPointer<vtkImageActor>::New();
  this->ResultActor->SetInput(VTKSegmentMask);
  this->RightRenderer->RemoveAllViewProps();
  this->RightRenderer->AddActor(ResultActor);
  this->RightRenderer->ResetCamera();
  this->Refresh();

  this->progressBar->hide();
}
*/

// Display segmented image with transparent background pixels
void GraphCutSegmentationWidget::slot_SegmentationComplete()
{
  // When the ProgressThread emits the StopProgressSignal, we need to display the result of the segmentation

  // Convert the segmentation mask to a binary VTK image
  vtkSmartPointer<vtkImageData> VTKSegmentMask =
    vtkSmartPointer<vtkImageData>::New();
  Helpers::ITKScalarImagetoVTKImage(this->GraphCut.GetSegmentMask(), VTKSegmentMask);

  // Convert the image into a VTK image for display
  vtkSmartPointer<vtkImageData> VTKImage =
    vtkSmartPointer<vtkImageData>::New();
  Helpers::ITKImagetoVTKImage(this->GraphCut.GetMaskedOutput(), VTKImage);

  vtkSmartPointer<vtkImageData> VTKMaskedImage =
    vtkSmartPointer<vtkImageData>::New();
  Helpers::MaskImage(VTKImage, VTKSegmentMask, VTKMaskedImage);

  // Remove the old output, set the new output and refresh everything
  //this->ResultActor = vtkSmartPointer<vtkImageActor>::New();
  this->ResultActor->SetInputData(VTKMaskedImage);
  this->RightRenderer->RemoveAllViewProps();
  this->RightRenderer->AddActor(ResultActor);
  this->RightRenderer->ResetCamera();
  this->Refresh();
}

float GraphCutSegmentationWidget::ComputeLambda()
{
  // Compute lambda by multiplying the percentage set by the slider by the MaxLambda set in the text box

  double lambdaMax = this->txtLambdaMax->text().toDouble();
  double lambdaPercent = this->sldLambda->value()/100.;
  double lambda = lambdaPercent * lambdaMax;

  return lambda;
}

void GraphCutSegmentationWidget::UpdateLambda()
{
  // Compute lambda and then set the label to this value so the user can see the current setting
  double lambda = ComputeLambda();
  this->lblLambda->setText(QString::number(lambda));
}

void GraphCutSegmentationWidget::sldHistogramBins_valueChanged()
{
  this->GraphCut.SetNumberOfHistogramBins(sldHistogramBins->value());
  //this->lblHistogramBins->setText(QString::number(sldHistogramBins->value())); // This is taken care of by a signal/slot pair setup in QtDesigner
}

void GraphCutSegmentationWidget::on_sldRGBWeight_valueChanged()
{
  this->lblRGBWeight->setText(QString::number(sldRGBWeight->value() / 100.));
}

void GraphCutSegmentationWidget::on_radForeground_clicked()
{
  this->GraphCutStyle->SetInteractionModeToForeground();
}

void GraphCutSegmentationWidget::on_radBackground_clicked()
{
  this->GraphCutStyle->SetInteractionModeToBackground();
}

void GraphCutSegmentationWidget::on_actionClearForegroundSelection_activated()
{
  this->GraphCutStyle->ClearForegroundSelections();
}

void GraphCutSegmentationWidget::on_actionClearBackgroundSelection_activated()
{
  this->GraphCutStyle->ClearBackgroundSelections();
}

void GraphCutSegmentationWidget::on_actionSaveForegroundSelection_activated()
{
  QString directoryName = QFileDialog::getExistingDirectory(this,
     "Open Directory", QDir::homePath(), QFileDialog::ShowDirsOnly);

  std::string foregroundFilename = QDir(directoryName).absoluteFilePath("foreground.png").toStdString();

  std::cout << "Writing to " << foregroundFilename << std::endl;

  UnsignedCharScalarImageType::Pointer foregroundImage = UnsignedCharScalarImageType::New();
  foregroundImage->SetRegions(this->ImageRegion);
  foregroundImage->Allocate();
  Helpers::IndicesToBinaryImage(this->GraphCutStyle->GetForegroundSelection(), foregroundImage);

  typedef  itk::ImageFileWriter< UnsignedCharScalarImageType  > WriterType;
  WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(foregroundFilename);
  writer->SetInput(foregroundImage);
  writer->Update();
}


void GraphCutSegmentationWidget::on_actionSaveBackgroundSelection_activated()
{
  QString directoryName = QFileDialog::getExistingDirectory(this,
     "Open Directory", QDir::homePath(), QFileDialog::ShowDirsOnly);

  std::string backgroundFilename = QDir(directoryName).absoluteFilePath("background.png").toStdString();

  std::cout << "Writing to " << backgroundFilename << std::endl;

  UnsignedCharScalarImageType::Pointer foregroundImage = UnsignedCharScalarImageType::New();
  foregroundImage->SetRegions(this->ImageRegion);
  foregroundImage->Allocate();
  Helpers::IndicesToBinaryImage(this->GraphCutStyle->GetForegroundSelection(), foregroundImage);

  typedef  itk::ImageFileWriter< UnsignedCharScalarImageType  > WriterType;
  WriterType::Pointer writer = WriterType::New();

  UnsignedCharScalarImageType::Pointer backgroundImage = UnsignedCharScalarImageType::New();
  backgroundImage->SetRegions(this->ImageRegion);
  backgroundImage->Allocate();
  Helpers::IndicesToBinaryImage(this->GraphCutStyle->GetBackgroundSelection(), backgroundImage);

  writer->SetFileName(backgroundFilename);
  writer->SetInput(backgroundImage);
  writer->Update();
}

void GraphCutSegmentationWidget::on_btnCut_clicked()
{
  // Get the number of bins from the slider
  this->GraphCut.SetNumberOfHistogramBins(this->sldHistogramBins->value());

  if(this->sldLambda->value() == 0)
    {
    QMessageBox msgBox;
    msgBox.setText("You must select lambda > 0!");
    msgBox.exec();
    return;
    }

  // Setup the graph cut from the GUI and the scribble selection
  this->GraphCut.SetRGBWeight(sldRGBWeight->value() / 100.);
  this->GraphCut.SetLambda(ComputeLambda());
  this->GraphCut.SetSources(this->GraphCutStyle->GetForegroundSelection());
  this->GraphCut.SetSinks(this->GraphCutStyle->GetBackgroundSelection());

  /////////////
  QFuture<void> future = QtConcurrent::run(this->GraphCut, &ImageGraphCut::PerformSegmentation);
  this->FutureWatcher.setFuture(future);

  this->ProgressDialog->setMinimum(0);
  this->ProgressDialog->setMaximum(0);
  this->ProgressDialog->setWindowModality(Qt::WindowModal);
  this->ProgressDialog->exec();

}

#if 0
void InnerWidget::actionFlip_Image_triggered()
{
  this->CameraUp[1] *= -1;
  this->LeftRenderer->GetActiveCamera()->SetViewUp(this->CameraUp);
  this->RightRenderer->GetActiveCamera()->SetViewUp(this->CameraUp);
  this->Refresh();
}
#endif

#if 0
void InnerWidget::actionSave_Segmentation_triggered()
{
  // Ask the user for a filename to save the segment mask image to

  QString fileName = QFileDialog::getSaveFileName(this,
    tr("Save Segment Mask Image"), "/home/doriad", tr("Image Files (*.png *.bmp)"));
/*
  // Convert the image from a 1D vector image to an unsigned char image
  typedef itk::CastImageFilter< GrayscaleImageType, itk::Image<itk::CovariantVector<unsigned char, 1>, 2 > > CastFilterType;
  CastFilterType::Pointer castFilter = CastFilterType::New();
  castFilter->SetInput(this->GraphCut->GetSegmentMask());

  typedef itk::NthElementImageAdaptor< itk::Image<itk:: CovariantVector<unsigned char, 1>, 2 >,
    unsigned char> ImageAdaptorType;

  ImageAdaptorType::Pointer adaptor = ImageAdaptorType::New();
  adaptor->SelectNthElement(0);
  adaptor->SetImage(castFilter->GetOutput());
*/
  // Write the file
  //typedef  itk::ImageFileWriter< ImageAdaptorType > WriterType;
  typedef  itk::ImageFileWriter< MaskImageType > WriterType;
  WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(fileName.toStdString());
  //writer->SetInput(adaptor);
  writer->SetInput(this->GraphCut->GetSegmentMask());
  writer->Update();

}
#endif

void GraphCutSegmentationWidget::OpenFile(const std::string& fileName)
{
  // Clear the scribbles
  this->GraphCutStyle->ClearSelections();

  // Read file
  itk::ImageFileReader<ImageType>::Pointer reader = itk::ImageFileReader<ImageType>::New();
  reader->SetFileName(fileName);
  reader->Update();

  this->ImageRegion = reader->GetOutput()->GetLargestPossibleRegion();

  this->GraphCut.SetImage(reader->GetOutput());

  // Convert the ITK image to a VTK image and display it
  vtkSmartPointer<vtkImageData> VTKImage = vtkSmartPointer<vtkImageData>::New();
  Helpers::ITKImagetoVTKImage(reader->GetOutput(), VTKImage);

  this->LeftRenderer->RemoveAllViewProps();

  this->OriginalImageActor->SetInputData(VTKImage);
  this->GraphCutStyle->InitializeTracer(this->OriginalImageActor);

  this->LeftRenderer->ResetCamera();
  this->Refresh();

  // Setup the scribble style
  if(this->radBackground->isChecked())
    {
    this->GraphCutStyle->SetInteractionModeToBackground();
    }
  else
    {
    this->GraphCutStyle->SetInteractionModeToForeground();
    }

  //std::cout << "Exit OpenFile()" << std::endl;
}

void GraphCutSegmentationWidget::Refresh()
{
  //this->LeftRenderer->Render();
  //this->RightRenderer->Render();
  this->qvtkWidgetRight->GetRenderWindow()->Render();
  this->qvtkWidgetLeft->GetRenderWindow()->Render();
  this->qvtkWidgetRight->GetInteractor()->Render();
  this->qvtkWidgetLeft->GetInteractor()->Render();
}