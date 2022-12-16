-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
macroScript AddGenomeModifier category:"Genome" tooltip:"Add Genome Modifier - hold SHIFT to create unique Genome modifiers if more than one object is selected. Without SHIFT, one Genome modifier will be instanced to all." icon:#("Genome",1)
(
	on isEnabled return (selection.count > 0 and GenomeModifier != undefined)
	on execute do 
	(
		max modify mode
		local theObjects = (for o in selection where findItem GeometryClass.classes (classof o) > 0 and classof o != TargetObject collect o)
		if keyboard.shiftPressed then
		(
			for o in theObjects do 
			(
				try
				(
					select o
					local newModifier = GenomeModifier()
					modPanel.addModToSelection newModifier
					newModifier.MagmaHolder.autoUpdate = true
				)catch()
			)
			select theObjects
		)	
		else
		(
			if theObjects.count == 1 and modPanel.getCurrentObject() != undefined then
			(
				local newModifier = GenomeModifier()
				if validModifier selection newModifier do
				(
					modPanel.addModToSelection newModifier
					newModifier.MagmaHolder.autoUpdate = true
					try(::GenomeMFEditor_Functions.OpenMagmaFlowEditor newModifier.MagmaHolder)catch()
				)
			)
			else
			(
				try
				(
					local newModifier = GenomeModifier()
					for o in theObjects do addModifier o newModifier
					max modify mode
					newModifier.MagmaHolder.autoUpdate = true
				)catch()
				try(::GenomeMFEditor_Functions.OpenMagmaFlowEditor newModifier.MagmaHolder)catch()				
			)
		)
	)	
)