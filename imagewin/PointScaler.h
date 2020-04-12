/*
Copyright (C) 2005 The Pentagram Team
Copyright (C) 2010 The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef POINTSCALER_H_INCLUDED
#define POINTSCALER_H_INCLUDED

#include "ArbScaler.h"

namespace Pentagram {

class PointScaler : public ArbScaler {
public:
	PointScaler();

	virtual uint32    ScaleBits() const;          //< bits for supported integer scaling
	virtual bool      ScaleArbitrary() const;     //< supports arbitrary scaling of any degree

	const char     *ScalerName() const override;         //< Name Of the Scaler (1 word)
	const char     *ScalerDesc() const override;         //< Desciption of the Scaler
	const char     *ScalerCopyright() const override;    //< Scaler Copyright info
};

}

#endif
