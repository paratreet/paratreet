/** @file TipsyFormat.h
 @author Graeme Lufkin (gwl@u.washington.edu)
 @date Created November 19, 2003
 @version 1.0
 */
 
#ifndef TIPSYFORMAT_H__klxdfkljre8434381198443698j3q6tb
#define TIPSYFORMAT_H__klxdfkljre8434381198443698j3q6tb

#include <string>

#include "Simulation.h"
#include "TipsyReader.h"

namespace SimulationHandling {

class TipsyFormatReader : public Simulation {
	Tipsy::TipsyReader r;
	Tipsy::header h;
public:
		
	TipsyFormatReader(const std::string& filename);

	virtual bool loadAttribute(const std::string& familyName, const std::string& attributeName, int64_t numParticles = 0, const u_int64_t startParticle = 0);
	
	bool loadFromTipsyFile(const std::string& filename);
};

} //close namespace SimulationHandling

#endif //TIPSYFORMAT_H__klxdfkljre8434381198443698j3q6tb
