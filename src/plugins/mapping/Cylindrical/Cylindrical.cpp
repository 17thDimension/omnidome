/* Copyright (c) 2014-2015 "Omnidome" by Michael Winkelmann
 * Dome Mapping Projection Software (http://omnido.me).
 * Omnidome was created by Michael Winkelmann aka Wilston Oreo (@WilstonOreo)
 *
 * This file is part of Omnidome.
 *
 * Omnidome is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Cylindrical.h"

#include <QDataStream>
#include <QOpenGLShaderProgram>
#include <omni/util.h>
#include <omni/ui/MappingParameters.h>
#include <omni/serialization/PropertyMap.h>

namespace omni
{
  namespace mapping
  {
    Cylindrical::Cylindrical()
    {
    }

    Cylindrical::~Cylindrical()
    {
    }

    QWidget* Cylindrical::widget() {
        return ui::makeWidget<ui::MappingParameters>(this);
    }
  }
}
