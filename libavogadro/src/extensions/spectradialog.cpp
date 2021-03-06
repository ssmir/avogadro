/**********************************************************************
  SpectraDialog - Visualize spectral data from QM calculations

  Copyright (C) 2009 by David Lonie

  This file is part of the Avogadro molecular editor project.
  For more information, see <http://avogadro.openmolecules.net/>

  This library is free software; you can redistribute it and/or modify
  it under the terms of the GNU Library General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 ***********************************************************************/

#include "spectradialog.h"
#include "spectratype.h"
#include "spectratype_ir.h"
#include "spectratype_nmr.h"

#include <QPen>
#include <QColor>
#include <QColorDialog>
#include <QButtonGroup>
#include <QDebug>
#include <QDoubleValidator>
#include <QFileDialog>
#include <QFontDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QFile>
#include <QDir>
#include <QPixmap>
#include <QSettings>
#include <QListWidgetItem>

#include <avogadro/molecule.h>
#include <avogadro/plotwidget.h>

#include <openbabel/mol.h>
#include <openbabel/obiter.h>
#include <openbabel/generic.h>

using namespace OpenBabel;
using namespace std;

namespace Avogadro {

  SpectraDialog::SpectraDialog( QWidget *parent, Qt::WindowFlags f ) :
      QDialog( parent, f )
  {
    ui.setupUi(this);

    // Set up spectra variables
    m_spectra_ir = new IRSpectra(this);
    m_spectra_nmr = new NMRSpectra(this);

    // Initialize vars
    m_schemes = new QList<QHash<QString, QVariant> >;

    // Hide advanced options initially
    ui.tab_widget->hide();

    // setting the limits for the plot
    ui.plot->setAntialiasing(true);
    ui.plot->setDefaultLimits( 4000.0, 400.0, 0.0, 100.0 );
    ui.plot->axis(PlotWidget::BottomAxis)->setLabel(tr("X Axis"));
    ui.plot->axis(PlotWidget::LeftAxis)->setLabel(tr("Y Axis"));
    m_calculatedSpectra = new PlotObject (Qt::red, PlotObject::Lines, 2);
    m_importedSpectra = new PlotObject (Qt::white, PlotObject::Lines, 2);
    m_nullSpectra = new PlotObject (Qt::white, PlotObject::Lines, 2); // Used to replace disabled plot objects
    ui.plot->addPlotObject(m_calculatedSpectra);	// index 0
    ui.plot->addPlotObject(m_importedSpectra);		// index 1

    // Scheme connections
    connect(ui.list_schemes, SIGNAL(currentRowChanged(int)),
            this, SLOT(updateScheme(int)));
    connect(ui.push_newScheme, SIGNAL(clicked()),
            this, SLOT(addScheme()));
    connect(ui.push_renameScheme, SIGNAL(clicked()),
            this, SLOT(renameScheme()));
    connect(ui.push_removeScheme, SIGNAL(clicked()),
            this, SLOT(removeScheme()));
    connect(ui.push_colorBackground, SIGNAL(clicked()),
            this, SLOT(changeBackgroundColor()));
    connect(ui.push_colorForeground, SIGNAL(clicked()),
            this, SLOT(changeForegroundColor()));
    connect(ui.push_colorCalculated, SIGNAL(clicked()),
            this, SLOT(changeCalculatedSpectraColor()));
    connect(ui.push_colorImported, SIGNAL(clicked()),
            this, SLOT(changeImportedSpectraColor()));
    connect(ui.push_font, SIGNAL(clicked()),
            this, SLOT(changeFont()));

    // Image export connections
    connect(ui.push_imageSave, SIGNAL(clicked()),
            this, SLOT(saveImage()));
    connect(ui.push_imageFilename, SIGNAL(clicked()),
            this, SLOT(saveImageFileDialog()));

    // Plot connections
    connect(ui.cb_import, SIGNAL(toggled(bool)),
            this, SLOT(toggleImported(bool)));
    connect(ui.cb_calculate, SIGNAL(toggled(bool)),
            this, SLOT(toggleCalculated(bool)));
    connect(ui.push_import, SIGNAL(clicked()),
            this, SLOT(importSpectra()));
    connect(ui.push_export, SIGNAL(clicked()),
            this, SLOT(exportSpectra()));

    // Misc. connections
    connect(ui.combo_spectra, SIGNAL(currentIndexChanged(QString)),
            this, SLOT(updateCurrentSpectra(QString)));
    connect(ui.push_customize, SIGNAL(clicked()),
            this, SLOT(toggleCustomize()));
    connect(ui.push_loadSpectra, SIGNAL(clicked()),
            this, SLOT(loadSpectra()));

    readSettings();
  }

  SpectraDialog::~SpectraDialog()
  {
    writeSettings();
    delete m_spectra_ir;
    delete m_spectra_nmr;
  }

  void SpectraDialog::setMolecule(Molecule *molecule)
  {
    if (m_molecule == molecule || !molecule) {
      return;
    }
    m_molecule = molecule;

    // set the filename in the image export widget
    QFileInfo defaultFile(m_molecule->fileName());
    QString defaultPath = defaultFile.canonicalPath();
    if (defaultPath.isEmpty()) {
      defaultPath = QDir::homePath();
    }
    ui.edit_imageFilename->setText(defaultPath + '/' + defaultFile.baseName() + ".png");

    // Empty the tab widget and spectra combo box when the molecule changes,
    // only adding in the entries appropriate to the molecule as needed
    ui.combo_spectra->clear();
    ui.tab_widget->clear();
    ui.tab_widget->addTab(ui.tab_appearance, tr("&Appearance"));
    ui.tab_widget->addTab(ui.tab_imageExport, tr("E&xport Image"));

    // Check for IR data
    bool hasIR = m_spectra_ir->checkForData(m_molecule);
    if (hasIR) {
      ui.combo_spectra->addItem(tr("Infrared", "Infrared spectra option"));
      ui.tab_widget->addTab(m_spectra_ir->getTabWidget(), tr("&Infrared Spectra Settings"));
    }

    // Check for NMR data
    bool hasNMR = m_spectra_nmr->checkForData(m_molecule);
    if (hasNMR) {
      ui.combo_spectra->addItem(tr("NMR", "NMR spectra option"));
      ui.tab_widget->addTab(m_spectra_nmr->getTabWidget(), tr("&NMR Spectra Settings"));
      m_spectra_nmr->setAtom(""); // Empty string will grab from current selection in the dialog.
    }

    // Change this when other spectra are added!!
    if (!hasIR && !hasNMR) { // Actions if there are no spectra loaded
      qWarning() << "SpectraDialog::setMolecule: No spectra available!";
      ui.combo_spectra->addItem(tr("No data"));
      ui.push_colorCalculated->setEnabled(false);
      ui.cb_calculate->setEnabled(false);
      ui.cb_calculate->setChecked(false);
      return;
    } else { // Actions for all spectra
      ui.push_colorCalculated->setEnabled(true);
      ui.cb_calculate->setEnabled(true);
      ui.cb_calculate->setChecked(true);
    }
    // Set the appearances tab to be opened by default
    ui.tab_widget->setCurrentIndex(0);
    updateCurrentSpectra( ui.combo_spectra->currentText() );
    regenerateCalculatedSpectra();
  }

  void SpectraDialog::writeSettings() const {
    QSettings settings; // Already set up in avogadro/src/main.cpp

    settings.setValue("spectra/image/width", ui.spin_imageWidth->value());
    settings.setValue("spectra/image/height", ui.spin_imageHeight->value());
    settings.setValue("spectra/image/units", ui.combo_imageUnits->currentIndex());
    settings.setValue("spectra/image/DPI", ui.spin_imageDPI->value());
    settings.setValue("spectra/image/optimizeFontSize", ui.cb_imageFontAdjust->isChecked());

    settings.setValue("spectra/currentScheme", m_scheme);
    settings.beginWriteArray("spectra/schemes");
    for (int i = 0; i < m_schemes->size(); ++i) {
      settings.setArrayIndex(i);
      ////////////////////////////////////////////////////////////////
      // FIXME: When we bump to Qt 4.5, change the following
      //      settings.setValue("scheme", m_schemes->at(i));
      settings.beginGroup("hash");
      QHashIterator<QString, QVariant> iter(m_schemes->at(i));
      while (iter.hasNext()) {
          iter.next();
          settings.setValue(iter.key(), iter.value());
        }
      settings.endGroup();
      ////////////////////////////////////////////////////////////////
    }
    settings.endArray();
  }

  void SpectraDialog::readSettings() {
    QSettings settings; // Already set up in avogadro/src/main.cpp

    ui.spin_imageWidth->setValue(settings.value("spectra/image/width", 21).toInt());
    ui.spin_imageHeight->setValue(settings.value("spectra/image/height", 10).toInt());
    ui.combo_imageUnits->setCurrentIndex(settings.value("spectra/image/units", 0).toInt());
    ui.spin_imageDPI->setValue(settings.value("spectra/image/DPI", 150).toInt());
    ui.cb_imageFontAdjust->setChecked(settings.value("spectra/image/optimizeFontSize", true).toBool());

    int scheme = settings.value("spectra/currentScheme", 0).toInt();
    int size = settings.beginReadArray("spectra/schemes");
    m_schemes = new QList<QHash<QString, QVariant> >;
    for (int i = 0; i < size; ++i) {
      settings.setArrayIndex(i);
      ////////////////////////////////////////////////////////////////
      // FIXME: QVariant::toHash() isn't around until Qt 4.5
      //      m_schemes->append(settings.value("scheme").toHash());
      settings.beginGroup("hash");
      QHash<QString, QVariant> hash;
      QStringList keys = settings.allKeys();
      foreach (const QString &key, settings.allKeys())
        hash[key] = settings.value(key);
      m_schemes->append(hash);
      settings.endGroup();
      ////////////////////////////////////////////////////////////////
      new QListWidgetItem(m_schemes->at(i)["name"].toString(), ui.list_schemes);
    }
    settings.endArray();

    // create scheme list if it doesn't already exist
    if (m_schemes->isEmpty()) {
      // dark
      QHash<QString, QVariant> dark;
      dark["name"] = tr("Dark");
      dark["backgroundColor"] = Qt::black;
      dark["foregroundColor"] = Qt::white;
      dark["calculatedColor"] = Qt::red;
      dark["importedColor"] = Qt::gray;
      dark["font"] = QFont();
      new QListWidgetItem(dark["name"].toString(), ui.list_schemes);
      m_schemes->append(dark);

      // light
      QHash<QString, QVariant> light;
      light["name"] = tr("Light");
      light["backgroundColor"] = Qt::white;
      light["foregroundColor"] = Qt::black;
      light["calculatedColor"] = Qt::red;
      light["importedColor"] = Qt::gray;
      light["font"] = QFont();
      new QListWidgetItem(light["name"].toString(), ui.list_schemes);
      m_schemes->append(light);

      // publication
      QHash<QString, QVariant> publication;
      publication["name"] = tr("Publication");
      publication["backgroundColor"] = Qt::white;
      publication["foregroundColor"] = Qt::black;
      publication["calculatedColor"] = Qt::black;
      publication["importedColor"] = Qt::gray;
      publication["font"] = QFont("Century Schoolbook L", 13);
      new QListWidgetItem(publication["name"].toString(), ui.list_schemes);
      m_schemes->append(publication);

      // handdrawn
      QHash<QString, QVariant> handdrawn;
      handdrawn["name"] = tr("Handdrawn");
      handdrawn["backgroundColor"] = Qt::white;
      handdrawn["foregroundColor"] = Qt::gray;
      handdrawn["calculatedColor"] = Qt::darkGray;
      handdrawn["importedColor"] = Qt::lightGray;
      handdrawn["font"] = QFont("Domestic Manners", 16);
      new QListWidgetItem(handdrawn["name"].toString(), ui.list_schemes);
      m_schemes->append(handdrawn);
    }
    updateScheme(scheme);
  }

  ///////////////////
  // Color schemes //
  ///////////////////

  void SpectraDialog::updateScheme(int scheme) {
    ui.list_schemes->setCurrentRow(scheme);
    if (m_scheme != scheme) {
      m_scheme = scheme;
      schemeChanged();
    }
  }

  void SpectraDialog::addScheme() {
    QHash<QString, QVariant> newScheme = m_schemes->at(m_scheme);
    newScheme["name"] = tr("New Scheme");
    new QListWidgetItem(newScheme["name"].toString(), ui.list_schemes);
    m_schemes->append(newScheme);
    schemeChanged();
  }

  void SpectraDialog::removeScheme() {
    if (m_schemes->size() <= 1) return; // Don't delete the last scheme!
    int ret = QMessageBox::question(this, tr("Confirm Scheme Removal"), tr("Really remove current scheme?"));
    if (ret == QMessageBox::Ok) {
      m_schemes->removeAt(m_scheme);
      delete (ui.list_schemes->takeItem(m_scheme));
    }
  }

  void SpectraDialog::renameScheme() {
    int idx = m_scheme;
    bool ok;
    QString text = QInputDialog::getText(this, tr("Change Scheme Name"),
                                         tr("Enter new name for current scheme:"), QLineEdit::Normal,
                                         m_schemes->at(m_scheme)["name"].toString(), &ok);
    if (ok) {
      (*m_schemes)[idx]["name"] = text;
      delete (ui.list_schemes->takeItem(idx));
      ui.list_schemes->insertItem(idx, m_schemes->at(idx)["name"].toString());
      updateScheme(idx);
    }
  }

  void SpectraDialog::changeBackgroundColor()
  {
    QColor current (m_schemes->at(m_scheme)["backgroundColor"].value<QColor>());
    QColor color = QColorDialog::getColor(current, this);//, tr("Select Background Color")); <-- Title not supported until Qt 4.5 bump.
    if (color.isValid() && color != current) {
      (*m_schemes)[m_scheme]["backgroundColor"] = color;
      schemeChanged();
    }
  }

  void SpectraDialog::changeForegroundColor()
  {
    QColor current (m_schemes->at(m_scheme)["foregroundColor"].value<QColor>());
    QColor color = QColorDialog::getColor(current, this);//, tr("Select Foreground Color")); <-- Title not supported until Qt 4.5 bump.
    if (color.isValid() && color != current) {
      (*m_schemes)[m_scheme]["foregroundColor"] = color;
      schemeChanged();
    }
  }

  void SpectraDialog::changeCalculatedSpectraColor()
  {
    QColor current (m_schemes->at(m_scheme)["calculatedColor"].value<QColor>());
    QColor color = QColorDialog::getColor(current, this);//, tr("Select Calculated Spectra Color")); <-- Title not supported until Qt 4.5 bump.
    if (color.isValid() && color != current) {
      (*m_schemes)[m_scheme]["calculatedColor"] = color;
      schemeChanged();
    }
  }

  void SpectraDialog::changeImportedSpectraColor()
  {
    QColor current (m_schemes->at(m_scheme)["importedColor"].value<QColor>());
    QColor color = QColorDialog::getColor(current, this);//, tr("Select Imported Spectra Color")); <-- Title not supported until Qt 4.5 bump.
    if (color.isValid() && color != current) {
      (*m_schemes)[m_scheme]["importedColor"] = color;
      schemeChanged();
    }
  }

  void SpectraDialog::changeFont()
  {
    bool ok;
    QFont current (m_schemes->at(m_scheme)["font"].value<QFont>());
    QFont font = QFontDialog::getFont(&ok, current, this);
    if (ok) {
      (*m_schemes)[m_scheme]["font"] = font;
      schemeChanged();
    }
  }

  void SpectraDialog::schemeChanged() {
    ui.plot->setBackgroundColor(m_schemes->at(m_scheme)["backgroundColor"].value<QColor>());
    ui.plot->setForegroundColor(m_schemes->at(m_scheme)["foregroundColor"].value<QColor>());
    ui.plot->setFont(m_schemes->at(m_scheme)["font"].value<QFont>());

    QPen currentPen (m_importedSpectra->linePen());
    currentPen.setColor(m_schemes->at(m_scheme)["importedColor"].value<QColor>());
    m_importedSpectra->setLinePen(currentPen);

    currentPen = (m_calculatedSpectra->linePen());
    currentPen.setColor(m_schemes->at(m_scheme)["calculatedColor"].value<QColor>());
    m_calculatedSpectra->setLinePen(currentPen);

  }

  ///////////////////////
  // Plot Manipulation //
  ///////////////////////

  void SpectraDialog::updateCurrentSpectra(const QString & text)
  {
    if (text.isEmpty()) return;
    m_spectra = text;
    if (currentSpectra()) currentSpectra()->setupPlot(ui.plot);
    // Regenerate spectra plot objects and redraw plot
    regenerateCalculatedSpectra();
    regenerateImportedSpectra();
    updatePlot();
  }

  void SpectraDialog::exportSpectra()
  {
    // Prepare filename
    QFileInfo defaultFile(m_molecule->fileName());
    QString defaultPath = defaultFile.canonicalPath();
    if (defaultPath.isEmpty()) {
      defaultPath = QDir::homePath();
    }
    QString defaultFileName = defaultPath + '/' + defaultFile.baseName() + ".tsv";
    QString filename 	= QFileDialog::getSaveFileName(this, tr("Export Calculated Spectrum"), defaultFileName, tr("Tab Separated Values (*.tsv)"));

    // Open file
    QFile file (filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      qWarning() << "Cannot open file " << filename << " for writing!";
      return;
    }
    QTextStream out(&file);
    if (currentSpectra()) out << currentSpectra()->getTSV();
    file.close();
  }

  void SpectraDialog::importSpectra()
  {
    // Setup filename
    QFileInfo defaultFile(m_molecule->fileName());
    QString defaultPath = defaultFile.canonicalPath();
    if (defaultPath.isEmpty()) {
      defaultPath = QDir::homePath();
    }
    QString defaultFileName = defaultPath + '/' + defaultFile.baseName() + ".tsv";
    QStringList filters;
    filters
      << tr("Tab Separated Values") + " (*.tsv)"
      << tr("Comma Separated Values") + " (*.csv)"
      << tr("JCAMP-DX") + " (*.jdx)"
      << tr("All Files") + " (* *.*)";

    QString filename 	= QFileDialog::getOpenFileName(this, tr("Import Spectra"),
                                                       defaultFileName, filters.join(";;"));

    // Open file
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      qWarning() << "Error opening file \"" << filename << "\".";
      return;
    }

    // get file extension
    QStringList tmp 	= filename.split('.');
    QString ext 	= tmp.at(tmp.size()-1);

    // Prepare lists
    QList<double> x, y;

    // Prepare stream
    QTextStream in(&file);

    // Pick delimiter
    QString delim;
    if      (ext == "tsv" || ext == "TSV")	delim = '\t';
    else if (ext == "csv" || ext == "CSV") 	delim = ',';
    else if (ext == "jdx" || ext == "JDX") 	delim = "\\s+"; // Regex finds whitespace
    else {					// Not a supported file type....
      QMessageBox::warning(this, tr("Spectra Import"), tr("Unknown extension: %1").arg(ext));
      return;
    }

    // Extract data
    while (!in.atEnd()) {
      QString line = in.readLine();
      if (line.trimmed().startsWith('#')) continue; 	//discard comments
      QStringList data = line.split(QRegExp(delim));
      if (data.size() < 2) {
        qWarning() << "SpectraDialog::importSpectra Skipping invalid line in file " << filename << ":\n\t\"" << line << "\"";
        continue;
      }
      if (data.at(0).toDouble() && data.at(1).toDouble()) {
        x.append(data.at(0).toDouble());
        y.append(data.at(1).toDouble());
      }
      else {
        qWarning() << "SpectraDialog::importSpectra Skipping entry as invalid:\n\t" << data;
        continue;
      }
    }

    // Set GUI options
    ui.push_colorImported->setEnabled(true);
    ui.cb_import->setEnabled(true);
    ui.cb_import->setChecked(true);

    // Update plot and plot objects
    if (currentSpectra()) currentSpectra()->setImportedData(x,y);
    regenerateImportedSpectra();
  }

  void SpectraDialog::loadSpectra()
  {
    // Setup filename
    QFileInfo defaultFile(m_molecule->fileName());
    QString defaultPath = defaultFile.canonicalPath();
    if (defaultPath.isEmpty()) {
      defaultPath = QDir::homePath();
    }
    QString defaultFileName = defaultPath + '/' + defaultFile.baseName();
    QStringList types;
    // Define data types here. Make sure to include "IR" for IR data and "NMR" for NMR data, etc. 
    // Put the default file extension in (*.ext), i.e. (.out)
    types
      << tr("PWscf IR data (*.out)", "Do not remove 'IR' or '(*.out)' -- needed for parsing later" );
    bool ok;
    QString type = QInputDialog::getItem(this, tr("Data Format"), tr("Format:", "noun, not verb"),
                                         types, 0, false, &ok);
    if (!ok) return;
    
    QStringList filters;
    filters
      << type
      << tr("All Files") + " (* *.*)";

    QString filename 	= QFileDialog::getOpenFileName(this, tr("Load Spectral Data"),
                                                       defaultFileName, filters.join(";;"));

    // Open file
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      qWarning() << "Error opening file \"" << filename << "\".";
      return;
    }

    // get file extension
    QStringList tmp 	= filename.split('.');
    QString ext 	= tmp.at(tmp.size()-1);

    // Prepare stream
    QTextStream in(&file);

    if (type.contains("IR")) { // We have IR data loaded
      // Set m_spectra
      m_spectra = "Infrared";

      // Prepare lists
      QList<double> x,y;
      
      // Set up some info by data type:
      QString delim;
      QString cue; // Skip to this line before reading data in
      int wavenumber_idx;
      int intensity_idx;
      if (type.contains("PWscf")) { // Plane wave self consistant field output
        delim 	= "\\s+"; // finds all whitespace
        cue	= "#  mode";
        wavenumber_idx	= 2;
        intensity_idx	= 4;
      }

      // Cue file
      while (!in.atEnd())
        if (in.readLine().contains(cue)) break;

      // Iterate through file
      int min = (wavenumber_idx < intensity_idx) ? wavenumber_idx : intensity_idx;
      while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.trimmed().startsWith('#')) continue; 	//discard comments
        QStringList data = line.split(QRegExp(delim));
        if (data.size() < min) {
          qWarning() << "SpectraDialog::importSpectra Skipping invalid line in file " << filename 
                     << ": Too few entries (need " << min << "\n\t\"" << line << "\"";
          continue;
        }
        if (data.at(wavenumber_idx).toDouble() && data.at(intensity_idx).toDouble()) { // Check for valid conversions and non-zero data
          x.append(data.at(wavenumber_idx).toDouble());
          y.append(data.at(intensity_idx).toDouble());
        }
        else {
          qWarning() << "SpectraDialog::importSpectra Skipping entry as invalid:\n\t" << data;
          continue;
        }
      }

      // Prepare a molecule with the data
      OpenBabel::OBMol *obmol = new OpenBabel::OBMol;
      std::vector< std::vector< OpenBabel::vector3 > > Lx;
      OpenBabel::OBVibrationData *obvib = new OpenBabel::OBVibrationData;
      obvib->SetData(Lx, x.toVector().toStdVector(), y.toVector().toStdVector());
      obmol->SetData(obvib);
      Molecule *mol = new Molecule;
      mol->setOBMol(obmol);

      if (currentSpectra()) currentSpectra()->checkForData(mol);
      delete mol;
    }
  }

  void SpectraDialog::saveImageFileDialog() {
    QStringList filters;
    filters
      << tr("Portable Network Graphics") + " (*.png)"
      << tr("jpeg") + " (*.jpg *.jpeg)"
      << tr("Tagged Image File Format") + " (*.tiff)"
      << tr("Windows Bitmap") + " (*.bmp)"
      << tr("Portable Pixmap") + " (*.ppm)"
      << tr("X11 Bitmap") + " (*.xbm)"
      << tr("X11 Pixmap") + " (*.xpm)"
      << tr("All Files") + " (*.*)";


    QString filename   = QFileDialog::getSaveFileName(this, tr("Save Spectra Image"),
                                                      ui.edit_imageFilename->text(),
                                                      filters.join(";;"));
    if (filename.isEmpty()) {
      return;
    }
    // get file extension
    QStringList tmp 	= filename.split('.');
    QString ext 	= tmp.at(tmp.size()-1);

    if (ext != "png" && ext != "PNG" &&
        ext != "jpg" && ext != "JPG" &&
        ext != "bmp" && ext != "BMP" &&
        ext != "ppm" && ext != "PPM" &&
        ext != "xbm" && ext != "XBM" &&
        ext != "xpm" && ext != "XPM" &&
        ext != "tiff" && ext != "TIFF" ) {
      qWarning() << "SpectraDialog::saveImageFileDialog Invalid file extension: " << ext;
      QMessageBox::warning(this, tr("Invalid Filename"), tr("Unknown extension: %1").arg(ext));
      return;
    }
    ui.edit_imageFilename->setText(filename);
  }

  void SpectraDialog::saveImage()
  {
    QString filename = ui.edit_imageFilename->text();
    double w,h,factor=1;
    int dpi;

    switch (ui.combo_imageUnits->currentIndex())
      {
      case 0: // cm
        factor = 0.01;
        break;
      case 1: // mm
        factor = 0.001;
        break;
      case 2: // in
        factor = 0.0254;
        break;
      case 4: // px
        factor = 1;
        break;
    }
    w = factor * ui.spin_imageWidth->value();
    h = factor * ui.spin_imageHeight->value();
    dpi = ui.spin_imageDPI->value();
    bool opt = ui.cb_imageFontAdjust->isChecked();
    if (!ui.plot->saveImage(filename, w, h, dpi, opt)) {
      qWarning() << "SpectraDialog::saveImage Error saving plot to " << filename;
      QMessageBox::warning(this, tr("Error"), tr("A problem occurred while writing file %1").arg(filename));
    } else {
      QMessageBox::information(this, tr("Success!"), tr("Image successfully written to %1").arg(filename));
    }
  }

  void SpectraDialog::toggleImported(bool state) {
    if (state) {
      ui.plot->replacePlotObject(1,m_importedSpectra);
    }
    else {
      ui.plot->replacePlotObject(1,m_nullSpectra);
    }
    updatePlot();
  }

  void SpectraDialog::toggleCalculated(bool state) {
    if (state) {
      ui.plot->replacePlotObject(0,m_calculatedSpectra);
    }
    else {
      ui.plot->replacePlotObject(0,m_nullSpectra);
    }
    updatePlot();
  }

  void SpectraDialog::toggleCustomize() {
    if (ui.tab_widget->isHidden()) {
      ui.push_customize->setText(tr("Customi&ze <<"));
      ui.tab_widget->show();
      QSize s = size();
      s.setHeight(s.height() + ui.tab_widget->size().height());
      resize(s);
    }
    else {
      ui.push_customize->setText(tr("Customi&ze >>"));
      QSize s = size();
      s.setHeight(s.height() - ui.tab_widget->size().height());
      resize(s);
      ui.tab_widget->hide();
    }
  }

  void SpectraDialog::regenerateCalculatedSpectra() {
    if (currentSpectra()) currentSpectra()->getCalculatedPlotObject(m_calculatedSpectra);
    updatePlot();
  }

  void SpectraDialog::regenerateImportedSpectra() {
    if (currentSpectra()) currentSpectra()->getImportedPlotObject(m_importedSpectra);
    updatePlot();
  }

  void SpectraDialog::updatePlot()
  {
    ui.plot->update();
  }

  SpectraType * SpectraDialog::currentSpectra() {
    if (m_spectra == "Infrared")
      return m_spectra_ir;
    else if (m_spectra == "NMR")
      return m_spectra_nmr;

    return NULL;
  }
}

