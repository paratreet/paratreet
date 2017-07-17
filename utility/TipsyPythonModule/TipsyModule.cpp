//TipsyModule.cpp

#include <boost/python/module.hpp>

#include "export_Vector3D.h"

void export_CodeUnits();
void export_OrientedBox();
void export_Sphere();
void export_TipsyParticles();
void export_TipsyReader();
void export_TipsyFile();
void export_SS();

BOOST_PYTHON_MODULE(TipsyFile) {
	
	export_CodeUnits();
	
	export_Vector3D<float>();
	export_OrientedBox();
	export_Sphere();
	
	export_TipsyParticles();
	export_TipsyReader();
	export_TipsyFile();
	
	export_Vector3D<double>("Vector3Dd");
	export_SS();
}
