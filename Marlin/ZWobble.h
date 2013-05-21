/*
  ZWobble.h - A class that manages ZWobble
 
  Copyright (c) 2013 Francesco Santini
  Additional code by Tealvince (source: http://www.soliforum.com/topic/1165/firmware-z-wobble-compensation/)
  
  Code loosely based on Neil Martin's hysteresis fix
 
 Grbl is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 Grbl is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with Grbl.  If not, see <http://www.gnu.org/licenses/>.
 */


/*
 * How to insert it in the firmware code (it requires the function set_position and copy_position in planner.h/cpp):
 * 
 * 1) include ZWobble.h in Marlin.pde
 * 2) insert DECLARE_ZWOBBLE_MCODES(REPORT_CODE, SET_CODE) inside Marlin.pde below the following lines:
     else if(code_seen('M'))
     {
	switch( (int)code_value() ) 
	{
	  
 * substituting REPORT_CODE with the Mcode value that makes the class report its state to serial and SET_CODE 
 * with the code that allows changing the parameters (good choices might be 96 and 97)
 * 
 * 3) include ZWobble.h in planner.cpp
 * 4) insert  zwobble.InsertCorrection(z); at the beginning of the function plan_buffer_line
 * 
 * How to use it:
 * 
 * Set the parameters with M97 A<Amplitude_in_mm> W<period_in_mm> P<phase_in_degrees>
 * 
 * A good value for the period is the thread step of the Z rod (in the Solidoodle2 it is 1.41)
 * The other two parameters need to be determined experimentally (Amplitude will be <0.1 typically)
 * 
 * KNOWN LIMITATION (by design): if you redefine the Z value during your print (with a G92 for example), the correction *will* screw up
 * 
 * An alternative possibility is to directly sample the Z axis with a dial indicator. In this case, it is needed to define a period with
 *  M97 W<period_in_mm>
 * and then add samples with the syntax
 *  M97 Z<zRod> H<zActual>
 * 
 * How does it work?
 * 
 * This class compensates for a wobble of the Z axis that makes the translation rod movement->bed (extruder) movement nonlinear.
 * Instead of assuming Zactual = Zrod, the function assumes that Zaxtual = Zrod + A*sin(w*Zrod + phase). Since the user wants to
 * specify Zactual, we need to invert the formula to obtain Zrod, which is the value that will serve as the input of the motor.
 * 
 * Unfortunately, the formula is not invertible analitically so I resorted to a lookup table approach that tabulates Zrod->Zactual
 * within a sine period, and I used a reverse lookup and linear interpolation to find the desired Zrod.
 * 
 * The size of the LUT is specified by the constant STEPS_IN_ZLUT.
 * 
 * The actual correction is by default made by "virtually" changing the Z position to a number that will make the next planned move
 * add or subtract the correct amount of steps to get from the origin Z position to the target Z position.
 * 
 * This correction is not applied if origin or target are within the first ZWOBBLE_MIN_Z millimiters.
 * 
 */

#ifndef _ZWOBBLE_H
#define _ZWOBBLE_H

#include "Configuration.h"

// uncomment the following to activate serial report
//#define ZWOBBLE_DEBUG

#ifndef STEPS_IN_ZLUT
  #define STEPS_IN_ZLUT 50
#endif

#ifndef ZWOBBLE_MIN_Z
  #define ZWOBBLE_MIN_Z 0.1
#endif

// define the following if the ZWobble correction should change the position. If undefined, it just returns the number of motor steps for the correction
#define ZWOBBLE_USE_FAKEPOS

#define DECLARE_ZWOBBLE_MCODES(REPORT_CODE, SET_CODE) 			\
   case REPORT_CODE: 							\
   {									\
     zwobble.ReportToSerial();						\
   }									\
   break;								\
   case SET_CODE:							\
   {									\
     if (code_seen('A')) zwobble.setAmplitude(code_value() );		\
     if (code_seen('W')) zwobble.setPeriod(code_value() );		\
     if (code_seen('P')) zwobble.setPhase(code_value() );		\
     float zVal = -1, hVal = -1, lVal = -1;                             \
     if (code_seen('Z')) zVal = code_value();                           \
     if (code_seen('H')) hVal = code_value();                           \
     if (code_seen('L')) lVal = code_value();                           \
     if (zVal >= 0 && hVal >= 0) zwobble.setSample( zVal, hVal );       \
     if (zVal >= 0 && lVal >= 0) zwobble.setScaledSample( zVal, lVal ); \
     if (lVal >  0 && hVal >  0) zwobble.setScalingFactor( hVal/lVal ); \
   }									\
   break;

//===========================================================================

class ZWobble
{
public:
  ZWobble( float _amplitude, float _period, float _phase );
  
  void Set( float _amplitude, float _period, float _phase );
  void ReportToSerial();
  long InsertCorrection(const float targetZ);
  
  void setAmplitude( float _amplitude );
  void setPeriod(float _period );
  void setPhase(float _phase );
  void setSample(float zRod, float zActual);
  void setScaledSample(float zRod, float zScaledLength);
  void setScalingFactor(float zActualPerScaledLength);
  void setVerbosity( boolean _verbosity );

private:
  float m_amplitude, m_puls, m_phase;
  boolean m_consistent;

  int lutSize;
  float zLut[STEPS_IN_ZLUT][2];
  void calculateLut();
  void initLinearLut();
  void insertInLut(float, float);
  float findInLut(float);
  float findZRod(float);
  boolean areParametersConsistent();

  float lastZ, lastZRod;
  float m_scalingFactor;
  boolean m_sinusoidal;

};

//===========================================================================

extern ZWobble zwobble;
extern long position[4]; // defined in planner.cpp

#endif

