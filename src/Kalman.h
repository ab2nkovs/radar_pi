/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Navico BR24 Radar Plugin
 * Author:   David Register
 *           Dave Cowell
 *           Kees Verruijt
 *           Douwe Fokkema
 *           Sean D'Epagnier
 ***************************************************************************
 *   Copyright (C) 2010 by David S. Register              bdbcat@yahoo.com *
 *   Copyright (C) 2012-2013 by Dave Cowell                                *
 *   Copyright (C) 2012-2016 by Kees Verruijt         canboat@verruijt.net *
 *   Copyright (C) 2013-2016 by Douwe Fokkkema             df@percussion.nl*
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************
 */

#ifndef _BR24KALMAN_H_
#define _BR24KALMAN_H_

#include "Matrix.h"
#include "RadarMarpa.h"
#include "br24radar_pi.h"

PLUGIN_BEGIN_NAMESPACE

//    Forward definitions
class LocalPosition;
class ArpaTarget;
class Polar;

class KalmanFilter {
 public:
  KalmanFilter();
  ~KalmanFilter();
  void SetMeasurement(Polar* p, LocalPosition* x, Polar* expected, int range);
  void Predict(LocalPosition* x, double delta_time);  // measured position and expected position
  void ResetFilter();

  Matrix<double, 4> A;
  Matrix<double, 4> AT;
  Matrix<double, 4, 2> W;
  Matrix<double, 2, 4> WT;
  Matrix<double, 2, 4> H;
  Matrix<double, 4, 2> HT;
  Matrix<double, 2> V;
  Matrix<double, 2> VT;
  Matrix<double, 4> P;
  Matrix<double, 2> Q;
  Matrix<double, 2> R;
  Matrix<double, 4, 2> K;
  Matrix<double, 4> I;
};

static Matrix<double, 4, 2> ZeroMatrix42;
static Matrix<double, 2, 4> ZeroMatrix24;
static Matrix<double, 4> ZeroMatrix4;

PLUGIN_END_NAMESPACE
#endif