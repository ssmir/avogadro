/**********************************************************************
  SurfaceDialog - Dialog for generating cubes and meshes

  Copyright (C) 2009 Marcus D. Hanwell

  This file is part of the Avogadro molecular editor project.
  For more information, see <http://avogadro.openmolecules.net/>

  Avogadro is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  Avogadro is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.
 **********************************************************************/

#include "surfacedialog.h"

#include <avogadro/glwidget.h>
#include <avogadro/engine.h>
#include <avogadro/molecule.h>

#include <QDebug>

namespace Avogadro {

  SurfaceDialog::SurfaceDialog(QWidget* parent, Qt::WindowFlags f)
    : QDialog(parent, f), m_glwidget(0), m_molecule(0), m_moIndex(-1),
    m_moColorIndex(-1)
  {
    ui.setupUi(this);
    ui.moCombo->hide();
    ui.moColorCombo->hide();

    // Initialize the surface and color by type mappings
    m_surfaceTypes << Cube::VdW << Cube::ESP;
    m_colorTypes << Cube::None << Cube::ESP;

    // Connect up some signals and slots
    connect(ui.calculateButton, SIGNAL(clicked()),
            this, SLOT(calculateClicked()));

    // Responses to some of the combo boxes changing
    connect(ui.surfaceCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(surfaceComboChanged(int)));
    connect(ui.colorByCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(colorByComboChanged(int)));
  }

  SurfaceDialog::~SurfaceDialog()
  {
  }

  void SurfaceDialog::setMOs(int num)
  {
    // If there are no MOs, then do nothing as there was a problem with the file
    if (num < 1)
      return;

    // Enable the MO combos and populate them
    ui.moCombo->setVisible(true);
    ui.moCombo->clear();
    ui.moColorCombo->setVisible(true);
    ui.moColorCombo->clear();
    // Add the orbitals to the combo boxes
    for (int i = 1; i <= num; ++i) {
      ui.moCombo->addItem(tr("MO %L1", "Molecular Orbital").arg(i));
      ui.moColorCombo->addItem(tr("MO %L1", "Molecular Orbital").arg(i));
    }
    // Now add the MO option to the surface and color combos
    ui.surfaceCombo->clear();
    ui.surfaceCombo->addItem(tr("Van der Waals", "Van der Waals surface type"));
    ui.surfaceCombo->addItem(tr("Electrostatic Potential",
                                "Electrostatic potential surface type"));
    ui.surfaceCombo->addItem(tr("Electron Density",
                                "Electron density surface type"));
    m_moIndex = ui.surfaceCombo->count();
    ui.surfaceCombo->addItem(tr("Molecular Orbital",
                                "Molecular orbital surface type"));

    ui.colorByCombo->clear();
    ui.colorByCombo->addItem(tr("Nothing", "No color mapping onto isosurface"));
    ui.colorByCombo->addItem(tr("Electrostatic Potential",
                                "Electrostatic potential surface type"));
    ui.colorByCombo->addItem(tr("Electron Density",
                                "Electron density surface type"));
    m_moColorIndex = ui.colorByCombo->count();
    ui.colorByCombo->addItem(tr("Molecular Orbital",
                                "Molecular orbital surface type"));

    // Update the type mappings too
    m_surfaceTypes.clear();
    m_surfaceTypes << Cube::VdW << Cube::ESP << Cube::ElectronDensity << Cube::MO;
    m_colorTypes.clear();
    m_colorTypes << Cube::None << Cube::ESP << Cube::ElectronDensity << Cube::MO;
  }

  void SurfaceDialog::setHOMO(int n)
  {
    ui.moCombo->setItemText(n, ui.moCombo->itemText(n) + ' '
                            + tr("(HOMO)", "Highest occupied MO"));
    ui.moCombo->setCurrentIndex(n);
    ui.moColorCombo->setItemText(n, ui.moColorCombo->itemText(n) + ' '
                                 + tr("(HOMO)", "Highest occupied MO"));
    ui.moColorCombo->setCurrentIndex(n);
  }

  void SurfaceDialog::setLUMO(int n)
  {
    ui.moCombo->setItemText(n, ui.moCombo->itemText(n) + ' '
                            + tr("(LUMO)", "Lowest unoccupied MO"));
    ui.moColorCombo->setItemText(n, ui.moColorCombo->itemText(n) + ' '
                                 + tr("(LUMO)", "Lowest unoccupied MO"));
  }

  double SurfaceDialog::stepSize()
  {
    // Return the step size depending upon the resolution requested
    switch (ui.resolutionCombo->currentIndex()) {
      case 0: // Low resolution
        return 0.35;
      case 1: // Medium resolution
        return 0.18;
      case 2: // High resolution
        return 0.10;
      case 3: // Very high resolution
        return 0.05;
      default:
        return 0.18;
    }
  }

  Engine * SurfaceDialog::currentEngine()
  {
    if (m_engines.size())
      return m_engines.at(ui.engineCombo->currentIndex());
    else
      return 0;
  }

  Cube::Type SurfaceDialog::cubeType()
  {
    return m_surfaceTypes.at(ui.surfaceCombo->currentIndex());
  }

  int SurfaceDialog::moNumber()
  {
    if (m_surfaceTypes.at(ui.surfaceCombo->currentIndex()) == Cube::MO)
      return ui.moCombo->currentIndex() + 1;
    else
      return -1;
  }

  Cube::Type SurfaceDialog::cubeColorType()
  {
    return m_colorTypes.at(ui.colorByCombo->currentIndex());
  }

  int SurfaceDialog::moColorNumber()
  {
    if (m_colorTypes.at(ui.colorByCombo->currentIndex()) == Cube::MO)
      return ui.moColorCombo->currentIndex() + 1;
    else
      return -1;
  }

  unsigned long SurfaceDialog::cubeFromFile() {
    if (m_surfaceTypes.at(ui.surfaceCombo->currentIndex()) == Cube::FromFile) {
      // Iterate through the cubes to find the loaded cube that is current
      QString text(ui.surfaceCombo->currentText());
      foreach (Cube *cube, m_molecule->cubes()) {
        if (text == cube->name())
          return cube->id();
      }
    }
    return FALSE_ID;
  }

  unsigned long SurfaceDialog::cubeColorFromFile() {
    if (m_colorTypes.at(ui.colorByCombo->currentIndex()) == Cube::FromFile) {
      // Iterate through the cubes to find the loaded cube that is current
      QString text(ui.colorByCombo->currentText());
      foreach (Cube *cube, m_molecule->cubes()) {
        if (text == cube->name())
          return cube->id();
      }
    }
    return FALSE_ID;
  }

  double SurfaceDialog::isoValue()
  {
    return ui.isoValueEdit->text().toDouble();
  }

  void SurfaceDialog::enableCalculation(bool enable)
  {
    ui.calculateButton->setEnabled(enable);
  }

  inline void SurfaceDialog::updateEngines()
  {
    // List the appropriate display types in the combo
    ui.engineCombo->clear();
    m_engines.clear();
    foreach (Engine *engine, m_glwidget->engines()) {
      if (engine->identifier() == "Surfaces") {
        m_engines.push_back(engine);
        ui.engineCombo->addItem(engine->alias());
      }
    }
  }

  void SurfaceDialog::setGLWidget(const GLWidget *gl)
  {
    // Disconnect signals and slots if we are switching to a different widget
    if (m_glwidget)
      disconnect(m_glwidget, 0, this, 0);

    m_glwidget = gl;
    updateEngines();

    // Now connect the signals and slots
    connect(m_glwidget, SIGNAL(engineAdded(Engine*)),
            this, SLOT(engineAdded(Engine*)));
    connect(m_glwidget, SIGNAL(engineRemoved(Engine*)),
            this, SLOT(engineRemoved(Engine*)));
  }

  void SurfaceDialog::setMolecule(const Molecule *mol)
  {
    // Disconnect signals from old molecule if there is one
    if (m_molecule)
      disconnect(m_molecule, 0, this, 0);

    m_molecule = mol;

    // Hide the MO combos
    ui.moCombo->hide();
    ui.moColorCombo->hide();

    // Reset the mapping and the combos too
    ui.surfaceCombo->clear();
    ui.surfaceCombo->addItem(tr("Van der Waals", "Van der Waals surface type"));
    ui.surfaceCombo->addItem(tr("Electrostatic Potential",
                                "Electrostatic potential surface type"));

    ui.colorByCombo->clear();
    ui.colorByCombo->addItem(tr("Nothing", "No color mapping onto isosurface"));
    ui.colorByCombo->addItem(tr("Electrostatic Potential",
                                "Electrostatic potential surface type"));

    // Update the type mappings too
    m_surfaceTypes.clear();
    m_surfaceTypes << Cube::VdW << Cube::ESP;
    m_colorTypes.clear();
    m_colorTypes << Cube::None << Cube::ESP;

    // Connect to the molecule signals to check for addition/removal of cubes
    connect(m_molecule, SIGNAL(primitiveAdded(Primitive *)),
            this, SLOT(updateCubes(Primitive *)));
    connect(m_molecule, SIGNAL(primitiveRemoved(Primitive *)),
            this, SLOT(updateCubes(Primitive *)));
    updateCubes(0);
  }

  void SurfaceDialog::updateCubes(Primitive *)
  {
    // This routine takes care of checking for loaded
    foreach (Cube *cube, m_molecule->cubes()) {
      if (cube->cubeType() == Cube::FromFile) {
        qDebug() << "Found one:" << cube->name();
        m_surfaceTypes.push_back(Cube::FromFile);
        ui.surfaceCombo->addItem(cube->name());
      }
    }
  }

  void SurfaceDialog::engineAdded(Engine *engine)
  {
    // If this is an orbital engine then append it to the list
    if (engine->identifier() == "Surfaces") {
      m_engines.push_back(engine);
      ui.engineCombo->addItem(engine->alias());
    }
  }

  void SurfaceDialog::engineRemoved(Engine *engine)
  {
    // If this is an orbital type then rebuild the engine list
    if (engine->identifier() == "Surfaces")
      updateEngines();
  }

  void SurfaceDialog::calculateClicked()
  {
    // This lets the extension know that a calculation has been requested
    emit calculate();
  }

  void SurfaceDialog::surfaceComboChanged(int n)
  {
    ui.moCombo->setEnabled(n == m_moIndex);
    if (m_surfaceTypes.size() > 0 && n >= 0 && n < m_surfaceTypes.size()) {
      ui.resolutionCombo->setEnabled(m_surfaceTypes[n] != Cube::FromFile);

      // Set a default isosurface value based upon the surface type
      double isoValue = 0.0;
      switch (m_surfaceTypes[n]) {
        case Cube::VdW:
          isoValue = 0.0;
          break;
        case Cube::ESP:
          isoValue = 0.05;
          break;
        case Cube::ElectronDensity:
          isoValue = 0.1;
          break;
        case Cube::MO:
          isoValue = 0.02;
          break;
        default:
          isoValue = 1.0;
      }
      ui.isoValueEdit->setText(QString::number(isoValue));
    }
  }

  void SurfaceDialog::colorByComboChanged(int n)
  {
    ui.moColorCombo->setEnabled(n == m_moColorIndex);
  }

}
