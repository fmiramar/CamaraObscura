// SPDX-License-Identifier: GPL-3.0-or-later
#include "CamaraObscura.hpp"

InterfaceTable* ft;

PluginLoad(CamaraObscura)
{
    ft = inTable;
    registerDarkVelvetReverb();
    registerGroupedFDN();
    registerVelvetFDN();
    registerRIRFDN();
    registerModalReverbBank();
    registerModalPlate();
    registerGeometryReverb();
}
