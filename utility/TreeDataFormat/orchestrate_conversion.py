#!/usr/bin/env python

import sys, os, glob, string

tipsy2tree_program = './tipsy2tree'
merge_tipsy_array_program = './merge_tipsy_array'

attributeMap = {
	'den': ['scalar', 'float32', 'smoothDensity'],
	'mvl': ['vector', 'float32', 'meanVelocity'],
	'spd': ['scalar', 'float32', 'speed'],
	'dsp': ['scalar', 'float32', 'velocityDispersion'],
	'dvv': ['scalar', 'float32', 'divVelocity'],
	'hsm': ['scalar', 'float32', 'smoothingLength'],
	'smoothTemperature': ['scalar', 'float32', 'smoothTemperature'],
	'polytropeTemperature': ['scalar', 'float32', 'Polytropic Temperature'],
	'gradTemperature': ['vector', 'float32', 'gradTemperature'],
	'udot': ['scalar', 'float32', 'Udot'],
	'u': ['scalar', 'float32', 'U'],
	'coolingTime': ['scalar', 'float32', 'coolingTime'],
	'k': ['scalar', 'float32', 'k'],
	'opacity': ['scalar', 'float32', 'opacity'],
	'fluxLimiter': ['scalar', 'float32', 'fluxLimiter'],
	'tau_up': ['scalar', 'float32', 'Tau Up'],
	'tau_down': ['scalar', 'float32', 'Tau Down'],
	'tau_effective': ['scalar', 'float32', 'Effective Tau'],
	'udot_radiation': ['scalar', 'float32', 'Udot (Radiation)'],
	'edge': ['scalar', 'float32', 'Edge Estimate'],
	'timestep': ['scalar', 'float32', 'Timestep Estimate'],
	'HI': ['scalar', 'float32', 'HI'],
	'HeI': ['scalar', 'float32', 'HeI'],
	'HeII': ['scalar', 'float32', 'HeII'],
	'Tform': ['scalar', 'float32', 'TemperatureAtFormationTime'],
	'igasorder': ['scalar', 'uint32', 'OriginalGasOrder'],
	'iord': ['scalar', 'uint32', 'OriginalOrder'],
	'massform': ['scalar', 'float32', 'MassAtFormationTime'],
	'rform': ['vector', 'float32', 'PositionAtFormationTime'],
	'rhoform': ['scalar', 'float32', 'DensityAtFormationTime'],
	'vform': ['vector', 'float32', 'VeclocityAtFormationTime'],
	'acc': ['vector', 'float32', 'acceleration'],
	'accg': ['vector', 'float32', 'accelerationGravity'],
	'dt': ['scalar', 'float32', 'timestep'],
	'SPHdt': ['scalar', 'float32', 'SPHtimestep'],
	'FeMassFrac': ['scalar', 'float32', 'FeMassFraction'],
	'OxMassFrac': ['scalar', 'float32', 'OxMassFraction'],
	'coolontime': ['scalar', 'float32', 'CoolOnTime'],
}

def main():
	if len(sys.argv) < 2:
		print 'Usage: orchestrate_conversion.py tipsyfile_base_name'
		return
		
	tipsyfile = sys.argv[1]
	newfile = os.path.basename(tipsyfile) + '.data'
	
	if os.path.exists(newfile):
		print 'Converted file appears to exist, will merge in new attributes'
	else:
		print 'Converting the main tipsy file ...'
		os.system('%s %s' % (tipsy2tree_program, tipsyfile))
	
	extrafiles = glob.glob(tipsyfile + '.*')
	for f in extrafiles:
		key = f[f.rfind('.') + 1:]
		if key in attributeMap:
			print 'Found an attribute file, %s' % key
			[dimensionality, datatype, attributeName] = attributeMap[key]
			mergedFiles = glob.glob(newfile + '/*/' + attributeName)
			if len(mergedFiles) == 0 or os.path.getmtime(f) > os.path.getmtime(mergedFiles[0]):
				print 'Attribute is new or more recent, merging it ...'
				os.system('%s %s %s %s %s "%s"' % (merge_tipsy_array_program, newfile, f, dimensionality, datatype, attributeName))
			
	
if __name__ == '__main__':
	main()
