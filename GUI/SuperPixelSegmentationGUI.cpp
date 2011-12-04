/*=========================================================================
 *
 *  Copyright David Doria 2011 daviddoria@gmail.com
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/

#include "SuperPixelSegmentationGUI.h"

// Custom
#include "Helpers.h"
#include "HelpersQt.h"
#include "SuperPixelSegmentationComputationObject.h"

// ITK
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkRGBPixel.h"
#include "itkScalarToRGBColormapImageFilter.h"

// Qt
#include <QIcon>
#include <QFileDialog>
#include <QGraphicsPixmapItem>

// Called by all constructors
void SuperPixelSegmentationGUI::DefaultConstructor()
{
  this->setupUi(this);

  this->MinSizeMin = 0;
  this->MinSizeMax = 100;
  this->sldMinSize->setMinimum(this->MinSizeMin);
  this->sldMinSize->setMaximum(this->MinSizeMax);
  
  this->KMin = 0.0f;
  this->KMax = 100.0f;
  this->sldK->SetMinValue(this->KMin);
  this->sldK->SetMaxValue(this->KMax);

  this->SigmaMin = 1.0f;
  this->SigmaMax = 10.0f;
  this->sldSigma->SetMinValue(this->SigmaMin);
  this->sldSigma->SetMaxValue(this->SigmaMax);
  
  this->progressBar->setMinimum(0);
  this->progressBar->setMaximum(0);
  this->progressBar->hide();

  this->ComputationThread = new SuperPixelSegmentationComputationThread;
  connect(this->ComputationThread, SIGNAL(StartProgressBarSignal()), this, SLOT(slot_StartProgressBar()));
  connect(this->ComputationThread, SIGNAL(StopProgressBarSignal()), this, SLOT(slot_StopProgressBar()));
  connect(this->ComputationThread, SIGNAL(IterationCompleteSignal()), this, SLOT(slot_IterationComplete()));

  this->Image = ImageType::New();
  this->LabelImage = LabelImageType::New();

  this->Scene = new QGraphicsScene;
  this->graphicsView->setScene(this->Scene);

  this->ImagePixmapItem = NULL;
  this->LabelImagePixmapItem = NULL;
}

// Default constructor
SuperPixelSegmentationGUI::SuperPixelSegmentationGUI()
{
  DefaultConstructor();
};

SuperPixelSegmentationGUI::SuperPixelSegmentationGUI(const std::string& imageFileName)
{
  DefaultConstructor();
  this->SourceImageFileName = imageFileName;
  
  OpenImage(this->SourceImageFileName);
}

void SuperPixelSegmentationGUI::showEvent ( QShowEvent * event )
{
  if(this->ImagePixmapItem)
    {
    this->graphicsView->fitInView(this->ImagePixmapItem, Qt::KeepAspectRatio);
    }
}

void SuperPixelSegmentationGUI::resizeEvent ( QResizeEvent * event )
{
  if(this->ImagePixmapItem)
    {
    this->graphicsView->fitInView(this->ImagePixmapItem, Qt::KeepAspectRatio);
    }
}

void SuperPixelSegmentationGUI::on_btnSegment_clicked()
{
  SuperPixelSegmentationComputationObject<ImageType, LabelImageType>* computationObject =
    new SuperPixelSegmentationComputationObject<ImageType, LabelImageType>;
  computationObject->K = this->sldK->GetValue();
  computationObject->Sigma = this->sldSigma->GetValue();
  computationObject->MinSize = this->sldMinSize->value();
  computationObject->Image = this->Image;
  computationObject->LabelImage = this->LabelImage;
  ComputationThread->SetObject(computationObject);
  ComputationThread->start();
}

void SuperPixelSegmentationGUI::on_actionSaveResult_activated()
{
  // Get a filename to save
  QString fileName = QFileDialog::getSaveFileName(this, "Save File", ".", "Image Files (*.jpg *.jpeg *.bmp *.png *.mha)");

  if(fileName.toStdString().empty())
    {
    std::cout << "Filename was empty." << std::endl;
    return;
    }

  Helpers::WriteImage<LabelImageType>(this->LabelImage, fileName.toStdString());
  this->statusBar()->showMessage("Saved result.");
}

void SuperPixelSegmentationGUI::OpenImage(const std::string& imageFileName)
{
  // Load and display image
  typedef itk::ImageFileReader<ImageType> ImageReaderType;
  ImageReaderType::Pointer imageReader = ImageReaderType::New();
  imageReader->SetFileName(imageFileName);
  imageReader->Update();

  Helpers::DeepCopy<ImageType>(imageReader->GetOutput(), this->Image);

  QImage qimageImage = HelpersQt::GetQImageRGBA<ImageType>(this->Image);
  this->ImagePixmapItem = this->Scene->addPixmap(QPixmap::fromImage(qimageImage));
  this->graphicsView->fitInView(this->ImagePixmapItem);
  Refresh();
}

void SuperPixelSegmentationGUI::on_actionOpenImage_activated()
{
  QString fileName = QFileDialog::getOpenFileName(this,
                    "OpenFile", ".", "All Files (*.*)");

  if(!fileName.isEmpty())
    {
    OpenImage(fileName.toStdString());
    }
}

void SuperPixelSegmentationGUI::on_chkShowInput_clicked()
{
  Refresh();
}

void SuperPixelSegmentationGUI::on_chkShowSegments_clicked()
{
  Refresh();
}

void SuperPixelSegmentationGUI::slot_StartProgressBar()
{
  this->progressBar->show();
}

void SuperPixelSegmentationGUI::slot_StopProgressBar()
{
  this->progressBar->hide();
}

void SuperPixelSegmentationGUI::slot_IterationComplete()
{
  //Helpers::WriteImage<LabelImageType>(this->LabelImage, "LabelImage.mha");
  //QImage qimage = HelpersQt::GetQImageScalar<LabelImageType>(this->LabelImage);

  typedef itk::Image<itk::RGBPixel<unsigned char>, 2> RGBImageType;
  typedef itk::ScalarToRGBColormapImageFilter<LabelImageType, RGBImageType> ColorMapFilterType;
  ColorMapFilterType::Pointer colorMapFilter = ColorMapFilterType::New();
  colorMapFilter->SetInput(this->LabelImage);
  colorMapFilter->SetColormap( ColorMapFilterType::Hot );
  colorMapFilter->Update();

  QImage qimage = HelpersQt::GetQImageRGB<RGBImageType>(colorMapFilter->GetOutput());
  if(this->LabelImagePixmapItem)
    {
    this->Scene->removeItem(this->LabelImagePixmapItem);
    }
  this->LabelImagePixmapItem = this->Scene->addPixmap(QPixmap::fromImage(qimage));

  Refresh();
}

void SuperPixelSegmentationGUI::Refresh()
{
  if(this->LabelImagePixmapItem)
    {
    this->LabelImagePixmapItem->setVisible(this->chkShowSegments->isChecked());
    }
  if(this->ImagePixmapItem)
    {
    this->ImagePixmapItem->setVisible(this->chkShowInput->isChecked());
    }
}
