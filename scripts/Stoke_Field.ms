-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
plugin Modifier Stoke_Field_From_Mesh_Converter
name: "Stoke Mesh To Field"	
classid:#(0x70fe92f7, 0xf6c202b)
extends: Stoke_Field_From_Mesh
replaceUi:true
(
	
	fn createPresetsRCMenu type:#renderpercent =
	(
		if selection[1] == undefined or classof (modpanel.getCurrentObject()) != Stoke_Field_From_Mesh_Converter do return false
		local isInDelegate = true
		case type of
		(
			#spacing: (
						presetName = "gridSpacing"
						theParameter = "Spacing"
			)
			#viewportreduce: (
						presetName = "viewportreduce"
						theParameter = "viewportreduce"
			)
			#iconsize: (
						presetName = "iconSize"
						theParameter = "iconSize"
			)			
			#bandwidthVoxels: (
						presetName = "bandwidthVoxels"
						theParameter = "bandwidthVoxels"
			)
		)
		local presetsList = #()
		local theKeys = for i in (getIniSetting (getDir #plugcfg + "//StokeFieldSimPreferences.ini") presetName  ) collect (execute i)
		sort theKeys 
		if theKeys.count == 0 then 
		(
			theKeys = case type of
			(
				#spacing: #(1.0,2.0,3.0,5.0,8.0,10.0,20.0,50.0)
				#viewportreduce: #(1,2,3,5,10,20,50,100)
				#iconSize: #(1.0,10.0,30.0,50.0,100.0)
				#bandwidthVoxels: #(3.0,5.0,10.0,20.0)
			)
			for i in theKeys do
				setIniSetting (getDir #plugcfg + "//StokeFieldSimPreferences.ini") presetName (i as string) (i as string)
		)
		for k in theKeys do
			if findItem presetsList theValue == 0 do append presetsList k

		local theDelText = if isInDelegate then "delegate." else ""
		theValue = execute ("(modpanel.getCurrentObject())." + theDelText + theParameter)
		global StokeField_Presets_RCMenu
		local txt = "rcmenu StokeField_Presets_RCMenu\n(\n"
		
		txt+= "fn updateUI = (\n"
		txt+= "try((modpanel.getCurrentObject()).params_Rollout.updateUI())catch()\n"
		txt+= ")\n"
		
		if findItem presetsList theValue == 0 do 
		(
			txt += "menuItem mnu_AddPreset \"Add "+ theValue as string+"\"\n" 
			txt += "on mnu_AddPreset picked do (setIniSetting (getDir #plugcfg + \"//StokeFieldSimPreferences.ini\") \""+ presetName + "\" \""+ theValue as string +"\" \""+ theValue as string +"\" \n updateUI())\n"
			txt += "separator spr_20\n" 
		)	
		
		cnt = 0
		for i in presetsList do
		(
			cnt += 1
			txt += "menuItem mnu_preset"+ cnt as string +" \"" + i as string + "\" \n" 
			txt += "on mnu_preset" + cnt as string + " picked do ( (modpanel.getCurrentObject())."+ theDelText + theParameter +" = "+ i as string +"\n"
			txt += "updateUI()\n" 
			txt += ")\n"
		)
		
		if findItem presetsList theValue != 0 do 
		(
			txt += "separator spr_100\n" 
			txt += "menuItem mnu_RemovePreset \"Remove "+ theValue as string+"\"\n" 
			txt += "on mnu_RemovePreset picked do delIniSetting (getDir #plugcfg + \"//StokeFieldSimPreferences.ini\") \""+ presetName +"\" \""+ theValue as string +"\" \n"
		)	
		txt += ")\n"
		execute txt
		true
	)		
	
	rollout params_rollout "Stoke Mesh To Field"
	(
		dropdownlist ddl_outputType "Output Type:" items:#("Level Set","Density Field")
		checkbox chk_IncludeCPT "Include CPT Field" tooltip:"Create an additional Closest Point Transform field\n\nIt will point at the closest surface point on the LevelSet."

		group "Grid Spacing"
		(
			spinner spn_spacing "Spacing:" range:[0.1,1000000.0,1.0] fieldwidth:43 scale:0.01 type:#worldunits  across:2 align:#right offset:[57,0]
			button btn_Spacing_Preset ">" width:18 height:16 align:#right offset:[7,0] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the Grid voxel size in world units."

			spinner spn_bandwidthvoxels "Bandwidth:" range:[3.0,1000000.0,3.0] fieldwidth:43 scale:1.0 type:#float  across:2 align:#right offset:[57,0]
			button btn_bandwidthvoxels_Preset ">" width:18 height:16 align:#right offset:[7,0] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the Grid voxel size in world units."
		)
		group "Viewport Display"
		(
			spinner spn_viewportReduce "Reduce:" range:[0,999,delegate.ViewportReduce] type:#integer fieldwidth:43 offset:[57,0] across:2 align:#right
			button btn_viewportReduce_Preset ">" width:18 height:16 align:#right offset:[7,0] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Request the reduction of the display samples by showing every Nth.\n\nNote that the system might reduce automatically beyond the requested value to maintain usability."
		
			spinner spn_iconSize "Icon Size:" range:[0.0,10000.0,delegate.iconSize] fieldwidth:43 across:2 offset:[57,0] tooltip:"Set the size of the STOKE Viewport Icon." type:#worldunits
			button btn_iconSize_Preset ">" width:18 height:16 align:#right offset:[7,0] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the size of the STOKE Viewport Icon."
		)
		
		on spn_spacing changed val do delegate.Spacing = val
		
		
		on spn_bandwidthvoxels changed val do delegate.BandWidthVoxels = val
		on spn_viewportReduce changed val do delegate.ViewportReduce = val
		on spn_iconSize changed val do delegate.IconSize = val
		
		on ddl_outputType selected itm do delegate.outputType = itm-1
		on chk_IncludeCPT changed state do delegate.IncludeCPT = state
			
		fn popupPresetMenu type =
		(
			if createPresetsRCMenu type:type do
				popUpMenu StokeField_Presets_RCMenu position:mouse.screenPos		
		)
		on btn_spacing_Preset pressed do popupPresetMenu #spacing
		on btn_spacing_Preset rightclick do popupPresetMenu #spacing				

		on btn_bandwidthvoxels_Preset pressed do popupPresetMenu #bandwidthvoxels
		on btn_bandwidthvoxels_Preset rightclick do popupPresetMenu #bandwidthvoxels

		on btn_viewportReduce_Preset pressed do popupPresetMenu #viewportReduce
		on btn_viewportReduce_Preset rightclick do popupPresetMenu #viewportReduce

		on btn_iconSize_Preset pressed do popupPresetMenu #iconSize
		on btn_iconSize_Preset rightclick do popupPresetMenu #iconSize
		
		
		fn updateUI=
		(
			spn_spacing.value = delegate.Spacing
			spn_bandwidthvoxels.value = delegate.bandwidthvoxels
			spn_viewportReduce.value = delegate.viewportReduce
			spn_iconSize.value = delegate.iconSize
			
			ddl_outputType.selection = delegate.outputType+1
			chk_IncludeCPT.state = delegate.IncludeCPT			
		)
		
		fn updateLayout = 
		(
			spn_spacing.pos.x = spn_bandwidthvoxels.pos.x = spn_viewportReduce.pos.x = spn_iconSize.pos.x = params_rollout.width - 36
			ddl_outputType.width = params_rollout.width - 14
			ddl_outputType.pos.x = 7
		)
		on params_rollout open do
		(
			if (maxVersion())[1] > 20000 do params_rollout.autoLayoutOnResize = ::Stoke_autoLayoutOnResizeOn	--set to the value of the global variable determined in the Stoke Statup script	if the version is higher than 2018			
			updateLayout()
			updateUI()
		)
	)
)

plugin Modifier Stoke_Field_DivergenceFree
name: "Stoke Field Fluid Motion"	
classid:#(0x70fe92f9, 0xf6c404d)
extends: Stoke_Field_Fluid_Motion
replaceUi:true
(
	
	fn createPresetsRCMenu type:#renderpercent =
	(
		if selection[1] == undefined or classof (modpanel.getCurrentObject()) != Stoke_Field_DivergenceFree do return false
		local isInDelegate = false
		case type of
		(
			#spacing: (
						presetName = "gridSpacing"
						theParameter = "DefaultSpacing"
						isTime = false
						isInDelegate = true
			)
		)
		local presetsList = #()
		local theKeys = for i in (getIniSetting (getDir #plugcfg + "//StokeFieldSimPreferences.ini") presetName  ) collect (execute i)
		sort theKeys 
		if theKeys.count == 0 then 
		(
			theKeys = case type of
			(
				#spacing: #(1.0,2.0,3.0,5.0,8.0,10.0,20.0,50.0)
			)
			for i in theKeys do
				setIniSetting (getDir #plugcfg + "//StokeFieldSimPreferences.ini") presetName (i as string) (i as string)
		)
		for k in theKeys do
			if findItem presetsList theValue == 0 do append presetsList k

		local theDelText = if isInDelegate then "delegate." else ""
		theValue = execute ("(modpanel.getCurrentObject())." + theDelText + theParameter)
		global StokeField_Presets_RCMenu
		local txt = "rcmenu StokeField_Presets_RCMenu\n(\n"
		
		txt+= "fn updateUI = (\n"
		txt+= "try((modpanel.getCurrentObject()).params_Rollout.updateUI())catch()\n"
		txt+= ")\n"
		
		if findItem presetsList theValue == 0 do 
		(
			txt += "menuItem mnu_AddPreset \"Add "+ theValue as string+"\"\n" 
			txt += "on mnu_AddPreset picked do (setIniSetting (getDir #plugcfg + \"//StokeFieldSimPreferences.ini\") \""+ presetName + "\" \""+ theValue as string +"\" \""+ theValue as string +"\" \n updateUI())\n"
			txt += "separator spr_20\n" 
		)	
		
		cnt = 0
		for i in presetsList do
		(
			cnt += 1
			txt += "menuItem mnu_preset"+ cnt as string +" \"" + i as string + "\" \n" 
			txt += "on mnu_preset" + cnt as string + " picked do ( (modpanel.getCurrentObject())."+ theDelText + theParameter +" = "+ i as string +"\n"
			txt += "updateUI()\n" 
			txt += ")\n"
		)
		
		if findItem presetsList theValue != 0 do 
		(
			txt += "separator spr_100\n" 
			txt += "menuItem mnu_RemovePreset \"Remove "+ theValue as string+"\"\n" 
			txt += "on mnu_RemovePreset picked do delIniSetting (getDir #plugcfg + \"//StokeFieldSimPreferences.ini\") \""+ presetName +"\" \""+ theValue as string +"\" \n"
		)	
		txt += ")\n"
		execute txt
		true
	)	
	
	rollout params_rollout "Stoke Field Fluid Motion"
	(
		edittext edt_inputChannel "Input Channel:" labelontop:true width:142
		checkbox chk_OverrideSpacing across:3 align:#left offset:[0,0] tooltip:"Check to override the incoming Grid Spacing with a custom value."
		spinner spn_spacing "Spacing:" range:[0.1,1000000.0,1.0] fieldwidth:43 scale:0.01 type:#worldunits align:#right offset:[33,0]
		button btn_Spacing_Preset ">" width:18 height:16 align:#right offset:[7,0] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the Override Grid Spacing value in world units.\n\nOnly used if the checkbox is checked, otherwise the incoming grid spacing will be reused."
		
		on edt_inputChannel entered txt do delegate.inputChannel = txt
		on chk_OverrideSpacing changed val do delegate.OverrideSpacing = val
		on spn_Spacing changed val do delegate.DefaultSpacing = val
			
		fn popupPresetMenu type =
		(
			if createPresetsRCMenu type:type do
				popUpMenu StokeField_Presets_RCMenu position:mouse.screenPos		
		)
		on btn_spacing_Preset pressed do popupPresetMenu #spacing
		on btn_spacing_Preset rightclick do popupPresetMenu #spacing				

		fn updateUI =
		(
			edt_inputChannel.text = delegate.inputChannel
			chk_OverrideSpacing.state = delegate.OverrideSpacing
			spn_Spacing.value = delegate.DefaultSpacing
			
		)
		
		fn updateLayout = 
		(
			edt_inputChannel.width = params_rollout.width - 14
			edt_inputChannel.pos.x = 7
			spn_spacing.pos.x = params_rollout.width - 36
		)
		
		on params_rollout open do
		(
			if (maxVersion())[1] > 20000 do params_rollout.autoLayoutOnResize = ::Stoke_autoLayoutOnResizeOn	--set to the value of the global variable determined in the Stoke Statup script	if the version is higher than 2018				
			updateLayout()
			updateUI()
		)
	)
	
	rollout about_rollout "About" rolledup:true
	(
		label lab_01 "This Stoke Field modifier"
		label lab_02 "takes the incoming Field"
		label lab_03 "and makes it divergence-free,"
		label lab_04 "producing a Simple Fluid Motion"
		label lab_05 "without any Sinks and Sources."
	)	
)


plugin Modifier Stoke_Field_Display_Modifier
name: "Stoke Field Display"	
classid:#(0x55f7a7b9, 0x62243b2d)
extends: Stoke_Field_Display 
replaceUi:true
(
	local VectorChannels = #()
	local ScalarChannels = #()
	
	fn createPresetsRCMenu type:#scale =
	(
		if selection[1] == undefined or classof (modpanel.getCurrentObject()) != Stoke_Field_Display_Modifier do return false
		local isInDelegate = false
		case type of
		(
			#scale: (
						presetName = "viewVectorScale"
						theParameter = "ViewportVectorScale"
						isInDelegate = false
			)
			#ViewportScalarMin: (
						presetName = "viewportScalarMin"
						theParameter = "ViewportScalarMin"
						isInDelegate = false
			)
			#ViewportScalarMax: (
						presetName = "viewportScalarMax"
						theParameter = "ViewportScalarMax"
						isInDelegate = false
			)
			
			#ViewportPointSize: (
						presetName = "ViewportPointSize"
						theParameter = "ViewportPointSize"
						isInDelegate = false			
			)
			
		)
		local presetsList = #()
		local theKeys = for i in (getIniSetting (getDir #plugcfg + "//StokeFieldSimPreferences.ini") presetName  ) collect (execute i)
		sort theKeys 
		if theKeys.count == 0 then 
		(
			theKeys = case type of
			(
				#scale: #(0.01,0.03,0.04,0.05,0.1,0.3,0.5,1.0,2.0,10.0,24.0,25.0,30.0)
				#ViewportScalarMin: #(-100.0,-10.0,-1.0,0.0)
				#ViewportScalarMax: #(0.0,1.0,10.0,100.0)
				#ViewportPointSize: #(1.0,2.0,3.0,5.0,10.0,25.0)
			)
			for i in theKeys do
				setIniSetting (getDir #plugcfg + "//StokeFieldSimPreferences.ini") presetName (i as string) (i as string)
		)
		for k in theKeys do
			if findItem presetsList theValue == 0 do append presetsList k

		local theDelText = if isInDelegate then "delegate." else ""
		theValue = execute ("(modpanel.getCurrentObject())." + theDelText + theParameter)
		global StokeField_Presets_RCMenu
		local txt = "rcmenu StokeField_Presets_RCMenu\n(\n"
		
		txt+= "fn updateUI = (\n"
		txt+= "try((modpanel.getCurrentObject()).params_Rollout.updateUI())catch()\n"
		txt+= ")\n"
		
		if findItem presetsList theValue == 0 do 
		(
			txt += "menuItem mnu_AddPreset \"Add "+ theValue as string+"\"\n" 
			txt += "on mnu_AddPreset picked do (setIniSetting (getDir #plugcfg + \"//StokeFieldSimPreferences.ini\") \""+ presetName + "\" \""+ theValue as string +"\" \""+ theValue as string +"\" \n updateUI())\n"
			txt += "separator spr_20\n" 
		)	
		
		cnt = 0
		for i in presetsList do
		(
			cnt += 1
			txt += "menuItem mnu_preset"+ cnt as string +" \"" + i as string + "\" \n" 
			txt += "on mnu_preset" + cnt as string + " picked do ( (modpanel.getCurrentObject())."+ theDelText + theParameter +" = "+ i as string +"\n"
			txt += "updateUI()\n" 
			txt += ")\n"
		)
		
		if findItem presetsList theValue != 0 do 
		(
			txt += "separator spr_100\n" 
			txt += "menuItem mnu_RemovePreset \"Remove "+ theValue as string+"\"\n" 
			txt += "on mnu_RemovePreset picked do delIniSetting (getDir #plugcfg + \"//StokeFieldSimPreferences.ini\") \""+ presetName +"\" \""+ theValue as string +"\" \n"
		)	
		txt += ")\n"
		execute txt
		true
	)			
	
	parameters params rollout:params_rollout
	(
		ViewportDisplayMode type:#Integer default:1 ui:ddl_ViewportDisplayMode
		ResolutionX type:#Integer default:10 ui:spn_ResolutionX
		ResolutionY type:#Integer default:10 ui:spn_ResolutionY
		ResolutionZ type:#Integer default:10 ui:spn_ResolutionZ

		SpacingX type:#float default:1 ui:spn_SpacingX
		SpacingY type:#float default:1 ui:spn_SpacingY
		SpacingZ type:#float default:1 ui:spn_SpacingZ
		
		spacingMode type:#Integer default:3 ui:ddl_spacingMode
		
		UseViewportScalarMin type:#boolean default:false ui:chk_UseViewportScalarMin
		UseViewportScalarMax type:#boolean default:false ui:chk_UseViewportScalarMax
		UseViewportPointSize type:#boolean default:false ui:chk_UseViewportPointSize
		ViewportScalarMin type:#float default:-100 ui:spn_ViewportScalarMin
		ViewportScalarMax type:#float default:0 ui:spn_ViewportScalarMax
		ViewportPointSize type:#float default:3 ui:spn_ViewportPointSize
		ViewportReduce type:#integer default:5 ui:spn_ViewportReduce
		
		ViewportScalarChannel type:#string default:"" --ui:edt_ViewportScalarChannel
		ViewportColorChannel type:#string default:"" --ui:edt_ViewportColorChannel
		
		ViewportVectorChannel type:#string default:"" --ui:edt_ViewportVectorChannel
		ViewportVectorScale type:#float default:1.0 ui:spn_ViewportVectorScale
		ViewportVectorNormalize type:#boolean default:false ui:chk_ViewportVectorNormalize
		
		lockSpinners type:#boolean default:true ui:chk_lockSpinners
		
		on ViewportDisplayMode set val do delegate.ViewportDisplayMode  = val
		on spacingMode set val do delegate.spacingMode  = val-1
			
		on ResolutionX set val do 
		(
			if lockSpinners do ResolutionY = ResolutionZ = val
			delegate.Resolution = #(val, ResolutionY, ResolutionZ)
		)
		on ResolutionY set val do 
		(
			if lockSpinners do ResolutionX = ResolutionZ = val
			delegate.Resolution = #(ResolutionX, val, ResolutionZ)
		)
		on ResolutionZ set val do 
		(
			if lockSpinners do ResolutionY = ResolutionX = val
			delegate.Resolution = #(ResolutionX, ResolutionY, val)		
		)
		
		on SpacingX set val do 
		(
			if lockSpinners do  SpacingY= SpacingZ = val
			delegate.Spacing = #(val, SpacingY, SpacingZ)			
		)
		on SpacingY set val do 
		(
			if lockSpinners do  SpacingX= SpacingZ = val
			delegate.Spacing = #(SpacingX, val, SpacingZ)
		)
		on SpacingZ set val do 
		(
			if lockSpinners do  SpacingY= SpacingX = val
			delegate.Spacing = #(SpacingX, SpacingY, val)
		)
			
		on UseViewportScalarMin set val do delegate.UseViewportScalarMin = val
		on UseViewportScalarMax set val do delegate.UseViewportScalarMax = val
		on UseViewportPointSize set val do delegate.UseViewportPointSize = val
		
		on ViewportScalarMin set val do 
		(
			delegate.ViewportScalarMin = val
			if val > ViewportScalarMax do ViewportScalarMax = val
		)
		on ViewportScalarMax set val do 
		(
			delegate.ViewportScalarMax = val
			if val < ViewportScalarMin do ViewportScalarMin = val
		)
		on ViewportPointSize set val do 
		(
			delegate.ViewportPointSize = val
		)
		on ViewportReduce set val do delegate.ViewportReduce = val
			
		on ViewportScalarChannel set val do 
		(
			delegate.ViewportScalarChannel = val
			redrawViews()
		)
		on ViewportVectorChannel set val do 
		(
			delegate.ViewportVectorChannel = val
			redrawViews()
		)
		on ViewportVectorScale set val do delegate.ViewportVectorScale = val
		on ViewportColorChannel set val do 
		(
			delegate.ViewportColorChannel = val
			redrawViews()
		)
			
		on ViewportVectorNormalize set val do delegate.ViewportVectorNormalize = val
		on ViewportVectorScale set val do delegate.ViewportVectorScale = val
	)
	
	rollout params_rollout "Stoke Field Display"
	(
		dropdownlist ddl_spacingMode "Spacing Mode:" items:#("Relative Resolution","Absolute Spacing","Native Resolution") offset:[0,-3] width:142
		
		label lbl_paceHolder01 ""
		spinner spn_ResolutionX "X Samples:" type:#integer fieldwidth:50 range:[1,10000,10] align:#right offset:[0,0]
		spinner spn_ResolutionY "Y Samples:" type:#integer fieldwidth:50 range:[1,10000,10] align:#right offset:[0,-2]
		spinner spn_ResolutionZ "Z Samples:" type:#integer fieldwidth:50 range:[1,10000,10] align:#right offset:[0,-2]
		
		groupBox grp_spacing "Relative Resolution" height:80 width:155 align:#center offset:[0,-80]
		
		spinner spn_SpacingX "X Spacing:" type:#worldunits fieldwidth:50 range:[0.01,10000,1] align:#right offset:[0,-64] scale:0.01
		spinner spn_SpacingY "Y Spacing:" type:#worldunits fieldwidth:50 range:[0.01,10000,1] align:#right offset:[0,-2] scale:0.01
		spinner spn_SpacingZ "Z Spacing:" type:#worldunits fieldwidth:50 range:[0.01,10000,1] align:#right offset:[0,-2] scale:0.01
			
		spinner spn_ViewportReduce "Reduce:" type:#integer fieldwidth:50 range:[0,999,0] align:#right offset:[0,-40]
		
		checkbutton chk_lockSpinners "[" height:55 width:18 align:#left offset:[-3,-42]

		dropdownlist ddl_ViewportDisplayMode "Viewport Display Mode:" items:#("Points","Vectors") offset:[0,5] width:142
		checkbox chk_UseViewportPointSize "Point Size:" across:3 align:#left offset:[0,-2]  tooltip:"Use the Point Size value.\n\nAvailable in 3ds Max 2015+ running\nNitrous with DirectX 9"
		spinner spn_ViewportPointSize "" fieldwidth:38 range:[1,10000,5] align:#right offset:[34,-2]  tooltip:"Set the Point Size when the checkbox is on.\n\nAvailable in 3ds Max 2015+ running\nNitrous with DirectX 9"
		button btn_ViewportPointSize_Preset ">" width:18 height:16 align:#right offset:[7,-2] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the Point Size to a preset value.\n\nAvailable in 3ds Max 2015+ running\nNitrous with DirectX 9"
		
		group "Scalar Channel Display"
		(
			--edittext edt_ViewportScalarChannel labelOnTop:true offset:[0,-2] 
			dropdownlist ddl_possibleScalarChannels offset:[0,-4] width:142
			checkbox chk_UseViewportScalarMin "Min:" across:3 align:#left offset:[0,-2] 
			spinner spn_ViewportScalarMin "" fieldwidth:48 range:[-10000,10000,0] align:#right offset:[34,-2] 
			button btn_ViewportScalarMin_Preset ">" width:18 height:16 align:#right offset:[7,-2] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the Scalar Min. Threshold to a preset value."
			
			checkbox chk_UseViewportScalarMax "Max:" across:3 align:#left offset:[0,-2] 
			spinner spn_ViewportScalarMax "" fieldwidth:48 range:[-10000,10000,0] align:#right offset:[34,-2] 
			button btn_ViewportScalarMax_Preset ">" width:18 height:16 align:#right offset:[7,-2] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the Scalar Max. Threshold to a preset value."
		)
		group "Color Channel Display"
		(
			--edittext edt_ViewportColorChannel  labelOnTop:true offset:[0,-2] 
			dropdownlist ddl_possibleColorChannels offset:[0,-4] width:142
		)			
		group "Vector Channel Display"
		(
			--edittext edt_ViewportVectorChannel labelOnTop:true offset:[0,-2] 
			dropdownlist ddl_possibleVectorChannels offset:[0,-4] width:142
			checkbox chk_ViewportVectorNormalize "Normalize Vector" offset:[0,-2] 
			spinner spn_ViewportVectorScale "Vector Scale:" fieldwidth:48 range:[0.001,1000,1] align:#right offset:[57,-2]  across:2
			button btn_Scale_Preset ">" width:18 height:16 align:#right offset:[7,-2] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the Vector Scale to a preset value."
		)
		
		fn popupPresetMenu type =
		(
			if createPresetsRCMenu type:type do
				popUpMenu StokeField_Presets_RCMenu position:mouse.screenPos		
		)
		on btn_ViewportScalarMin_Preset pressed do popupPresetMenu #ViewportScalarMin
		on btn_ViewportScalarMin_Preset rightclick do popupPresetMenu #ViewportScalarMin
			
		on btn_ViewportScalarMax_Preset pressed do popupPresetMenu #ViewportScalarMax
		on btn_ViewportScalarMax_Preset rightclick do popupPresetMenu #ViewportScalarMax		
		
		on btn_ViewportPointSize_Preset pressed do popupPresetMenu #ViewportPointSize
		on btn_ViewportPointSize_Preset rightclick do popupPresetMenu #ViewportPointSize		
		
		
		
		on btn_Scale_Preset pressed do popupPresetMenu #scale
		on btn_Scale_Preset rightclick do popupPresetMenu #scale
	
		
		fn updateControlVisibility =
		(
			spn_ResolutionX.visible = spn_ResolutionY.visible = spn_ResolutionZ.visible = spacingMode == 1
			spn_SpacingX.visible = spn_SpacingY.visible = spn_SpacingZ.visible = spacingMode == 2
			spn_ViewportReduce.visible = spacingMode == 3
			chk_lockSpinners.visible = spacingMode != 3
			grp_spacing.caption = case spacingMode of
			(
				default: "Relative Resolution"
				2: "Absolute Spacing"
				3: "Native Resolution"
			)
			chk_UseViewportPointSize.enabled = spn_ViewportPointSize.enabled = btn_ViewportPointSize_Preset.enabled = StokeGlobalInterface.ViewportType == #Nitrous and StokeGlobalInterface.PointSizeSupported
		)
		
		fn updateChannels =
		(
			VectorChannels = #("<None>")
			ScalarChannels = #("<None>")			
			local theInterface = stokeGlobalInterface.CreateMXSField selection[1] StackIndex:((modPanel.getModifierIndex selection[1] (modPanel.getCurrentObject()))-1)
			local theChannels = theInterface.Channels
			for aChannel in theChannels do
			(
				theChannel = (filterString aChannel " []")
				if theChannel[3] == "1" do append ScalarChannels theChannel[1]
				if theChannel[3] == "3" do append VectorChannels theChannel[1]
			)				
			theInterface.Clear()
			local temp = deepCopy ScalarChannels
			ScalarAndVectorChannels = join VectorChannels (deleteItem temp 1)
			
			ddl_possibleScalarChannels.items = ScalarChannels
			theIndex = findItem ScalarChannels ViewportScalarChannel
			if theIndex == 0 do theIndex = 1
			ddl_possibleScalarChannels.selection = theIndex
			
			ddl_possibleVectorChannels.items =  VectorChannels
			theIndex = findItem VectorChannels ViewportVectorChannel
			if theIndex == 0 do theIndex = 1
			ddl_possibleVectorChannels.selection = theIndex
			
			ddl_possibleColorChannels.items = ScalarAndVectorChannels
			theIndex = findItem ScalarAndVectorChannels ViewportColorChannel
			if theIndex == 0 do theIndex = 1
			ddl_possibleColorChannels.selection = theIndex
			
		)

		on ddl_spacingMode selected itm do updateControlVisibility()
		on ddl_possibleScalarChannels selected itm do ViewportScalarChannel = if itm == 1 then "" else ddl_possibleScalarChannels.selected
		on ddl_possibleVectorChannels selected itm do ViewportVectorChannel = if itm == 1 then "" else ddl_possibleVectorChannels.selected
		on ddl_possibleColorChannels selected itm do ViewportColorChannel = if itm == 1 then "" else ddl_possibleColorChannels .selected
		
		fn updateLayout =
		(
			grp_spacing.width = params_rollout.width-8
			grp_spacing.pos.x = 4
			
			ddl_ViewportDisplayMode.width = ddl_spacingMode.width = ddl_possibleScalarChannels.width = ddl_possibleVectorChannels.width = ddl_possibleColorChannels.width = params_rollout.width-14
			ddl_ViewportDisplayMode.pos.x = ddl_spacingMode.pos.x = ddl_possibleScalarChannels.pos.x = ddl_possibleVectorChannels.pos.x = ddl_possibleColorChannels.pos.x = 8
			
			spn_ViewportPointSize.pos.x = params_rollout.width-36
			spn_ViewportVectorScale.pos.x = spn_ViewportScalarMin.pos.x = spn_ViewportScalarMax.pos.x =params_rollout.width-36
		)
		
		on params_rollout open do
		(
			if (maxVersion())[1] > 20000 do params_rollout.autoLayoutOnResize = ::Stoke_autoLayoutOnResizeOn	--set to the value of the global variable determined in the Stoke Statup script	if the version is higher than 2018				
			updateLayout()
			updateControlVisibility()	
			updateChannels()
		)
	)
)

plugin Geometry Stoke_Field
name:"Field Magma"
category:"Stoke"
classid:#(0x2f405148, 0x5aedb1e4)
extends: Stoke_Field_Static
replaceui:true
(
	local params_rollout
	local LastErrorID = -100
	local LastErrorMsg = undefined
	
	parameters params rollout:params_rollout
	(
		BoundsMinX type:#float default:-2 ui:spn_boundsMinX
		BoundsMaxX type:#float default:2 ui:spn_boundsMaxX
		BoundsMinY type:#float default:-2 ui:spn_boundsMinY
		BoundsMaxY type:#float default:2 ui:spn_boundsMaxY
		BoundsMinZ type:#float default:0 ui:spn_boundsMinZ
		BoundsMaxZ type:#float default:4 ui:spn_boundsMaxZ
	
		GridSpacing type:#float animatable:false default:1 ui:spn_GridSpacing
		
		LimitToBounds type:#boolean default:true ui:chk_limitToBounds
		
		muteAllOutputs type:#boolean default:false animatable:false ui:chk_muteAllOutputs
		
		on boundsMinX set val do 
		(
			delegate.boundsMin.x = val
			if val > boundsMaxX-GridSpacing*2 do delegate.boundsMax.x = boundsMaxX = val + GridSpacing*2
			params_rollout.updateVoxelGridInfo()
		)
		on boundsMaxX set val do 
		(
			delegate.boundsMax.x = val
			if val < boundsMinX+GridSpacing*2 do delegate.boundsMin.x = boundsMinX = val - GridSpacing*2
			params_rollout.updateVoxelGridInfo()
		)
		on boundsMinY set val do 
		(
			delegate.boundsMin.y = val
			if val > boundsMaxY-GridSpacing*2 do delegate.boundsMax.y = boundsMaxY = val + GridSpacing*2
			params_rollout.updateVoxelGridInfo()
		)
		on boundsMaxY set val do 
		(
			delegate.boundsMax.y = val
			if val < boundsMinY+GridSpacing*2 do delegate.boundsMin.y = boundsMinY = val - GridSpacing*2
			params_rollout.updateVoxelGridInfo()
		)
		on boundsMinZ set val do 
		(
			delegate.boundsMin.z = val
			if val > boundsMaxZ-GridSpacing*2 do delegate.boundsMax.z = boundsMaxZ = val + GridSpacing*2
			params_rollout.updateVoxelGridInfo()
		)
		on boundsMaxZ set val do 
		(
			delegate.boundsMax.z = val
			if val < boundsMinZ+GridSpacing*2 do delegate.boundsMin.z =  boundsMinZ = val - GridSpacing*2
			params_rollout.updateVoxelGridInfo()
		)		
			
		on GridSpacing set val do 
		(
			delegate.GridSpacing = val
			params_rollout.updateVoxelGridInfo()
		)
		
		on LimitToBounds set val do
		(
			delegate.LimitToBounds = val
		)
	)
	parameters viewparams rollout:display_rollout
	(
		minManipulatorSize type:#float default:0.1 animatable:false ui:spn_minManipulatorSize
		maxManipulatorSize type:#float default:10.0 animatable:false ui:spn_maxManipulatorSize
	)
	
	fn createPresetsRCMenu type:#renderpercent =
	(
		if selection[1] == undefined or classof selection[1].baseobject != Stoke_Field do return false
		local isTime = false
		local isInDelegate = false
		case type of
		(
			#gridspacing: (
						presetName = "gridSpacing"
						theParameter = "gridSpacing"
						isTime = false
			)
			#viewportreduce: (
						presetName = "viewportreduce"
						theParameter = "viewportreduce"
						isTime = false
						isInDelegate = true
			)
			#iconsize: (
						presetName = "iconSize"
						theParameter = "iconSize"
						isTime = false
						isInDelegate = true
			)			
			#minManipulatorSize: (
						presetName = "minManipulatorSize"
						theParameter = "minManipulatorSize"
						isTime = false
						isInDelegate = false
			)
			#maxManipulatorSize: (
						presetName = "maxManipulatorSize"
						theParameter = "maxManipulatorSize"
						isTime = false
						isInDelegate = false
			)
			
			#viewVectorScale: (
						presetName = "viewVectorScale"
						theParameter = "ViewportVectorScale"
						isTime = false
						isInDelegate = true
			)
		)
		local presetsList = #()
		local theKeys = for i in (getIniSetting (getDir #plugcfg + "//StokeFieldSimPreferences.ini") presetName  ) collect (execute i)
		sort theKeys 
		if theKeys.count == 0 then 
		(
			theKeys = case type of
			(
				#gridspacing: #(1.0,2.0,3.0,5.0,8.0,10.0,20.0,50.0)
				#viewportreduce: #(1,2,3,5,10,20,50,100)
				#iconSize: #(1.0,10.0,30.0,50.0,100.0)
				#minManipulatorSize: #(0.1,1.0,2.0,5.0,10.0)
				#maxManipulatorSize: #(5.0,10.0,20.0,50.0)
				#viewVectorScale: #(0.01,0.03,0.04,0.05,0.1,0.3,0.5,1.0,2.0,10.0,24.0,25.0,30.0)
			)
			for i in theKeys do
				setIniSetting (getDir #plugcfg + "//StokeFieldSimPreferences.ini") presetName (i as string) (i as string)
		)
		for k in theKeys do
			if findItem presetsList theValue == 0 do append presetsList k

		local theDelText = if isInDelegate then "delegate." else ""
		theValue = execute ("selection[1]." + theDelText + theParameter)
		global StokeField_Presets_RCMenu
		local txt = "rcmenu StokeField_Presets_RCMenu\n(\n"
		
		txt+= "fn updateUI = (\n"
		txt+= "try(selection[1].params_Rollout.updateUI())catch()\n"
		txt+= "try(selection[1].display_rollout.updateUI())catch()\n"
		
		txt+= ")\n"
		
		if isTime do
		(
			txt += "menuItem mnu_StartFrame \""+ (animationrange.start.frame as integer) as string+" - Start Frame \"\n" 
			txt += "on mnu_StartFrame picked do (selection[1]."+  theParameter +" = "+ (animationrange.start.frame as integer) as string +"\nupdateUI())\n"

			txt += "menuItem mnu_CurrentFrame \""+ (sliderTime.frame as integer) as string+" - Current Frame\"\n" 
			txt += "on mnu_CurrentFrame picked do (selection[1]."+ theParameter +" = "+ (sliderTime.frame as integer) as string +"\nupdateUI())\n"

			txt += "menuItem mnu_EndFrame \""+ (animationrange.end.frame as integer) as string+" - End Frame\"\n" 
			txt += "on mnu_EndFrame picked do (\nselection[1]."+ theParameter +" = "+ (animationrange.end.frame as integer) as string +"\nupdateUI())\n"
			
			txt += "separator spr_10\n"
		)	
		if type == #gridspacing do
		(
			local theDefaultValue = (length ([boundsMaxX,boundsMaxY,boundsMaxZ ]-[boundsMinX,boundsMinY,boundsMinZ])) / 50.0
			txt += "menuItem mnu_defaultSpacing \"Default "+ theDefaultValue as string+"\"\n" 
			txt += "on mnu_defaultSpacing picked do (\nselection[1]."+ theParameter +" = "+ theDefaultValue as string +"\nupdateUI())\n"
			txt += "separator spr_15\n" 			
		)
			
		if findItem presetsList theValue == 0 do 
		(
			txt += "menuItem mnu_AddPreset \"Add "+ theValue as string+"\"\n" 
			txt += "on mnu_AddPreset picked do (setIniSetting (getDir #plugcfg + \"//StokeFieldSimPreferences.ini\") \""+ presetName + "\" \""+ theValue as string +"\" \""+ theValue as string +"\" \n updateUI())\n"
			txt += "separator spr_20\n" 
		)	
		
		cnt = 0
		for i in presetsList do
		(
			cnt += 1
			txt += "menuItem mnu_preset"+ cnt as string +" \"" + i as string + "\" \n" 
			txt += "on mnu_preset" + cnt as string + " picked do ( selection[1]."+ theDelText + theParameter +" = "+ i as string +"\n"
			txt += "updateUI()\n" 
			txt += ")\n"
		)
		
		if findItem presetsList theValue != 0 do 
		(
			txt += "separator spr_100\n" 
			txt += "menuItem mnu_RemovePreset \"Remove "+ theValue as string+"\"\n" 
			txt += "on mnu_RemovePreset picked do delIniSetting (getDir #plugcfg + \"//StokeFieldSimPreferences.ini\") \""+ presetName +"\" \""+ theValue as string +"\" \n"
		)	
		txt += ")\n"
		execute txt
		true
	)	
	
	fn createPresetMenu =
	(
		local theUserFolder = (dotnetclass "System.Environment").GetFolderPath (dotnetclass "System.Environment+SpecialFolder").LocalApplicationData + "\\Thinkbox\\StokeField\\MagmaFlows\\"
		makedir (theUserFolder) all:true
		local theFiles = getFiles (theUserFolder +"\\*.MagmaScript")
		local ss = stringStream ""
		format "global StokeFieldFlowPresetMenu \n" to:ss
		format "rcmenu StokeFieldFlowPresetMenu \n(\n" to:ss
		
		format "menuItem mnu_openFDV \"OPEN Stoke Field Data VIEWER...\"\n" to:ss
		format "on mnu_openFDV picked do \n" to:ss
		format "try(fileIn (StokeGlobalInterface.HomeDirectory+\"/Scripts/Stoke_FieldDataViewer.ms\"))catch()\n" to:ss
--		format "separator sep_10\n" to:ss
		format "menuItem mnu_openFDP \"OPEN Stoke Field Data PROBE...\"\n" to:ss
		format "on mnu_openFDP picked do \n" to:ss
		format "Macros.run \"Stoke\" \"StokeFieldProbe\"\n" to:ss
--		format "separator sep_20\n" to:ss
		format "menuItem mnu_openFDE \"OPEN Stoke Field Data EXPORTER...\"\n" to:ss
		format "on mnu_openFDE picked do \n" to:ss
		format "Macros.run \"Stoke\" \"StokeFieldDataExporter\"\n" to:ss
		format "separator sep_30\n" to:ss
		
		format "menuItem mnu_explore \"EXPLORE Magma Presets Folder...\"\n" to:ss
		format "separator sep_50\n" to:ss
		format "on mnu_explore picked do (shelllaunch @\"%\" \"\")\n"  (theUserFolder) to:ss		
		
		for i = 1 to theFiles.count do
		(
			format "menuItem mnu_% \"%\"\n" i (getFileNameFile theFiles[i]) to:ss
			format "on mnu_% picked do (\n" i  to:ss
			format "local theMagma = (modPanel.getCurrentObject()).delegate.MagmaHolder\n" to:ss
			format "theMagma.Reset()\n" to:ss
			format "local theEditor = ::StokeFieldStatic_ImplementationObject.openEditor theMagma (modPanel.getCurrentObject()) offscreen:true\n" to:ss
			format "::StokeFieldMFEditor_Functions.loadPreset @\"%\" theMagma (modPanel.getCurrentObject())\n" thefiles[i] to:ss
			format "theEditor.exposeControlsToModifier()\n" to:ss
			format "destroyDialog theEditor\n" to:ss
			format ")\n" to:ss
		)
		format ")\n" to:ss
		--print ss
		::StokeFieldFlowPresetMenu = execute (ss as string)
	)		
	
	
	rollout helpRollout "Help" rolledup:true
	(
		label lbl_about10 "STOKE FIELD MAGMA" align:#center offset:[0,-3]
		edittext edt_version "" align:#center readOnly:true align:#center offset:[-2,0] fieldwidth:110		
		
		dotNetControl btn_openOnlineHelp "Button" text:"Open Online Help..." width:152 height:22 align:#center offset:[0,0] enabled:true
		label lbl_logLevel "Log Level:" across:2 align:#left offset:[-5,0]
		dropdownlist ddl_loggingLevel items:#("None","Error","Warning","Progress","Stats","Debug") width:98 align:#right offset:[9,-3]
		
		on ddl_loggingLevel selected itm do
		(
			StokeGlobalInterface.LoggingLevel = ddl_loggingLevel.selected as name
			setIniSetting (getDir #plugcfg + "//StokePreferences.ini") "Log" "Level" (StokeGlobalInterface.LoggingLevel as string)
		)
		
		on btn_openOnlineHelp click arg do 
		(
			shellLaunch "https://docs.thinkboxsoftware.com/products/stoke/2.3/1_Documentation/manual/fields/stoke-mx-field-magma-object.html" ""
		)
		
		fn updateLayout =
		(
			btn_openOnlineHelp.FlatStyle = btn_openOnlineHelp.FlatStyle.System
			btn_openOnlineHelp.width = helpRollout.width-8
			btn_openOnlineHelp.pos.x = 4
			
			ddl_loggingLevel.width = helpRollout.width-64
			ddl_loggingLevel.pos.x = 60
		)
		
		on helpRollout open do
		(
			if (maxVersion())[1] > 20000 do helpRollout.autoLayoutOnResize = ::Stoke_autoLayoutOnResizeOn	--set to the value of the global variable determined in the Stoke Statup script	if the version is higher than 2018
			updateLayout()
			edt_version.text = try("Version " + StokeGlobalInterface.Version)catch("")
			ddl_loggingLevel.selection = findItem (for i in ddl_loggingLevel.items collect i as name) StokeGlobalInterface.LoggingLevel
		)
	)	
	
	rollout params_rollout "Field Magma Settings"
	(
		progressbar prg_MagmaError value:100 color:green width:8 height:30 offset:[-10,0] across:3 align:#left
		dotNetControl btn_openMagma "Button" text:"Open Magma Editor..." width:132 height:30 offset:[0,0] align:#center
		button btn_MagmaPreset ">" width:16 height:30 offset:[10,0] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) align:#right
		
		checkbox chk_autoUpdate "Auto-Update" checked:(delegate.magmaHolder.autoUpdate) offset:[-4,-3]  across:2
		checkbox chk_muteAllOutputs "Mute All" checked:(delegate.magmaHolder.autoUpdate) offset:[10,-3]  align:#right
		
		on chk_muteAllOutputs changed state do
		(
			local magma = delegate.magmaHolder
			local theNodes = delegate.magmaHolder.getNodes()
			for i in theNodes where magma.getNodeType i == "Output" do
			(
				if state then
				(
					magma.DeclareExtensionProperty i "PreMutedState" 
					magma.setNodeProperty i "PreMutedState" (magma.getNodeProperty i "enabled")
					magma.setNodeProperty i "enabled" false
				)
				else
				(
					local oldState = magma.getNodeProperty i "PreMutedState"
					if oldState == undefined do oldState = true
					magma.setNodeProperty i "enabled" oldState
				)
			)
		)
		
		on btn_MagmaPreset pressed do
		(
			createPresetMenu()
			popupMenu ::StokeFieldFlowPresetMenu pos:mouse.screenpos
		)
		
		on chk_autoUpdate changed state do 
		(
			delegate.magmaHolder.autoUpdate = state
			local theRollout = undefined
			for i in MagmaFlowEditor_CurrentEditors where i[1] == delegate.magmaHolder do theRollout = i[2]
			if theRollout != undefined do 
				theRollout.chk_autoUpdateModifier.state = state
		)
		
		on btn_openMagma click arg do ::StokeFieldStatic_ImplementationObject.openEditor delegate.MagmaHolder this
		
		group "World Space Grid Size:"
		(
			checkbutton chk_enableGridManipulator ">Enable Grid Manipulator" width:146 offset:[0,-3] checked:manipulateMode align:#center
			
			spinner spn_boundsMinX "X" range:[-(10^8),10^8,1.0] fieldwidth:60 enabled:true across:2 offset:[-5,0] align:#left type:#worldunits
			spinner spn_boundsMaxX "" range:[-(10^8),10^8,1.0] fieldwidth:60 enabled:true offset:[7,0] align:#right type:#worldunits
			
			spinner spn_boundsMinY "Y" range:[-(10^8),10^8,1.0] fieldwidth:60 enabled:true across:2 offset:[-5,0] align:#left type:#worldunits
			spinner spn_boundsMaxY "" range:[-(10^8),10^8,1.0] fieldwidth:60 enabled:true offset:[7,0] align:#right type:#worldunits
			 
			spinner spn_boundsMinZ "Z" range:[-(10^8),10^8,1.0] fieldwidth:60 enabled:true across:2 offset:[-5,0] align:#left type:#worldunits
			spinner spn_boundsMaxZ "" range:[-(10^8),10^8,1.0] fieldwidth:60 enabled:true offset:[7,0] align:#right type:#worldunits
			
			spinner spn_GridSpacing "Spacing " range:[0.01,10000,1.0] fieldwidth:43 enabled:true across:2 type:#worldunits align:#right offset:[57,0]
			button btn_GridSpacing_Preset ">" width:18 height:16 align:#right offset:[7,0] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the Grid voxel size in world units."
			
			edittext edt_voxelres "Grid:" offset:[6,0] align:#right fieldwidth:110 readonly:true
			edittext edt_voxcount "Count:" offset:[6,-5] align:#right fieldwidth:110 readonly:true
			
			checkbox chk_limitToBounds "Limit Field To Bounds" offset:[-4,-2] tooltip:"When checked, procedural fields will be defined only within the Grid bounds.\n\nWhen unchecked, procedural fields without Grid-based Nodes and Modifiers will also be defined outside the Grid."
		)

		dotNetControl btn_getGridSizeFromSelection "Button" text:"Get Grid Size From Selection..." width:154 height:20 align:#center offset:[0,-2] tooltip:"Pin the modifier stack, then select one or more objects and press this button to set the Grid Size to the combined World Oriented Bounding Box of the selection..."
		
		dotNetControl btn_centerGridToIcon	"Button" text:"Grid To Icon" width:77 align:#left offset:[-9,-3] across:2 height:20 tooltip:"Move the current Grid's Center to the Icon's Position."
		dotNetControl btn_centerIconToGrid	"Button" text:"Icon To Grid" width:77 align:#right offset:[9,-3] height:20 tooltip:"Move the Icon to the Center of the current Grid ."
		dotNetControl btn_centerGridBaseToIcon "Button" text:"Base To Icon" width:77 align:#left offset:[-9,-5] across:2 height:20 tooltip:"Move the current Grid's Base Center to the Icon's Position."
		dotNetControl btn_centerIconToGridBase "Button" text:"Icon To Base" width:77 align:#right offset:[9,-5] height:20 tooltip:"Move the Icon to the Center of the current Grid's Base."
		
		dotNetControl btn_dependentNodes "Button" text:"Select Dependent Object..." width:154 height:20 align:#center offset:[0,-2] tooltip:"Select an object that depends on this Field Magma object..."
		
		on btn_getGridSizeFromSelection click arg do
		(
			local theSel = for i in selection where i.baseobject != this collect i
			if theSel.count > 0 then
			(
				local theBBoxMin = [1,1,1]*(10^6)
				local theBBoxMax = [-1,-1,-1]*(10^6)
				for o in theSel do
				(
					if o.min.x < theBBoxMin.x do theBBoxMin.x = o.min.x
					if o.min.y < theBBoxMin.y do theBBoxMin.y = o.min.y
					if o.min.z < theBBoxMin.z do theBBoxMin.z = o.min.z
					if o.max.x > theBBoxMax.x do theBBoxMax.x = o.max.x
					if o.max.y > theBBoxMax.y do theBBoxMax.y = o.max.y
					if o.max.z > theBBoxMax.z do theBBoxMax.z = o.max.z
				)
				local theBBoxSize = theBBoxMax-theBBoxMin
				delegate.GridSpacing = gridSpacing = (length theBBoxSize)/52.0
			
				BoundsminX=theBBoxMin.x - gridSpacing
				BoundsminY=theBBoxMin.y - gridSpacing
				BoundsminZ=theBBoxMin.z  - gridSpacing
				BoundsmaxX=theBBoxMax.x + gridSpacing
				BoundsmaxY=theBBoxMax.y  + gridSpacing
				BoundsmaxZ=theBBoxMax.z + gridSpacing
				max views redraw
			)
		)
		
		on btn_dependentNodes click arg do
		(
			global StokeField_SelectDependentObject_RCMenu 
			local txt = "" as stringStream
			format "rcmenu StokeField_SelectDependentObject_RCMenu (\n" to:txt
			local theDepNodes = (refs.dependentNodes (refs.dependentNodes this)[1])
			if theDepNodes.count > 0 then
			(
				for i = 1 to theDepNodes.count do
				(
					format "menuItem itm_% \"Select '%' (%)\"" i theDepNodes[i].name (classof theDepNodes[i]) to:txt
					format "on itm_% picked do select (getNodeByName \"%\") \n" i theDepNodes[i].name  to:txt
				)
			)
			else
			(
				format "menuItem itm_1 \"No Dependent Objects Found!\" \n"  to:txt
			)
			format ")\n" to:txt
			execute (txt as string)
			popupmenu StokeField_SelectDependentObject_RCMenu pos:mouse.screenpos
		)	
		
		on btn_centerGridToIcon click arg do
		(
			theIconPos = selection[1].pos
			theBoxDiagonal = (([boundsMaxX,boundsMaxY,boundsMaxZ]-[boundsMinX,boundsMinY,boundsMinZ])/2)
			boundsMaxX = theIconPos.x + theBoxDiagonal.x
			boundsMaxY = theIconPos.y + theBoxDiagonal.y
			boundsMaxZ = theIconPos.z + theBoxDiagonal.z
			boundsMinX = theIconPos.x - theBoxDiagonal.x
			boundsMinY = theIconPos.y - theBoxDiagonal.y
			boundsMinZ = theIconPos.z - theBoxDiagonal.z
			redrawViews()
		)
		on btn_centerGridBaseToIcon click arg do
		(
			theIconPos = selection[1].pos
			theBoxDiagonal = (([boundsMaxX,boundsMaxY,boundsMaxZ]-[boundsMinX,boundsMinY,boundsMinZ])/2)
			boundsMaxX = theIconPos.x + theBoxDiagonal.x
			boundsMaxY = theIconPos.y + theBoxDiagonal.y
			boundsMaxZ = theIconPos.z + theBoxDiagonal.z*2
			boundsMinX = theIconPos.x - theBoxDiagonal.x
			boundsMinY = theIconPos.y - theBoxDiagonal.y
			boundsMinZ = theIconPos.z
			redrawViews()
		)	
		on btn_centerIconToGrid click arg do
		(
			selection[1].pos =delegate.boundsMin+ (delegate.boundsMax - delegate.boundsMin)/2
			redrawViews()
		)		
		on btn_centerIconToGridBase click arg do
		(
			local theBox = delegate.boundsMin+ (delegate.boundsMax - delegate.boundsMin)/2
			selection[1].pos = [theBox.x,theBox.y, boundsMinZ]
			redrawViews()
		)		
		
		on chk_enableGridManipulator changed state do manipulateMode = state
		
		fn addCommas txt =
		(
			if matchPattern txt pattern:"*L" do txt = substring txt 1 (txt.count-1)
			if matchPattern txt pattern:"*P" do txt = substring txt 1 (txt.count-1)
			theDot = findString txt "."
			if theDot == undefined then 
			(
				newTxt = "" 
				
				theDot = txt.count
			)	
			else 
			(
				newTxt = substring txt theDot -1
				theDot -= 1
			)
			cnt = 0
			for i = theDot to 1 by -1 do
			(
				cnt +=1
				newTxt = txt[i] + newTxt
				if cnt == 3 and i > 1 and txt[i-1] != "-" do 
				(
					newTxt = "," + newTxt
					cnt = 0
				)	
			)
			newTxt
		)		
		
		fn updateVoxelGridInfo =
		(
			theGridSize = ([boundsMaxX,boundsMaxY,boundsMaxZ ]-[boundsMinX,boundsMinY,boundsMinZ]) / Gridspacing
			edt_voxelres.text = (theGridSize.x as integer) as string + "x" +(theGridSize.y as integer) as string + "x" +(theGridSize.z as integer) as string  
			edt_voxcount.text = addCommas ((theGridSize.x as Integer64 * theGridSize.y as Integer64 * theGridSize.z as Integer64) as string)
		)
		
		fn popupPresetMenu type =
		(
			if createPresetsRCMenu type:type do
				popUpMenu StokeField_Presets_RCMenu position:mouse.screenPos		
			updateVoxelGridInfo()
		)
		on btn_gridspacing_Preset pressed do popupPresetMenu #gridspacing
		on btn_gridspacing_Preset rightclick do popupPresetMenu #gridspacing		
			
		fn updateLayout =
		(
			btn_getGridSizeFromSelection.FlatStyle = btn_dependentNodes.FlatStyle = btn_centerIconToGrid.FlatStyle = btn_centerIconToGridBase.FlatStyle = btn_centerGridBaseToIcon.FlatStyle = btn_centerGridToIcon.FlatStyle =  btn_openMagma.FlatStyle = btn_openMagma.FlatStyle.System
			btn_openMagma.width = params_rollout.width-32
			btn_openMagma.pos.x = 12
			
			chk_enableGridManipulator.pos.x = 8
			try(chk_enableGridManipulator.width = params_rollout.width-16)catch()
			
			spn_boundsMinX.pos.x = spn_boundsMinY.pos.x = spn_boundsMinZ.pos.x = params_rollout.width-90
			spn_GridSpacing.pos.x = params_rollout.width-32
			
			btn_getGridSizeFromSelection.width = btn_dependentNodes.width = params_rollout.width-6
			btn_getGridSizeFromSelection.pos.x = btn_dependentNodes.pos.x = 3
			
			
			btn_centerIconToGrid.width = btn_centerIconToGridBase.width = btn_centerGridBaseToIcon.width = btn_centerGridToIcon.width = (params_rollout.width-6)/2
			
			btn_centerIconToGrid.pos.x = btn_centerIconToGridBase.pos.x = 3+(params_rollout.width-6)/2
			btn_centerGridBaseToIcon.pos.x = btn_centerGridToIcon.pos.x = 3
			
		)
		
		on params_rollout open do
		(
			if (maxVersion())[1] > 20000 do params_rollout.autoLayoutOnResize = ::Stoke_autoLayoutOnResizeOn	--set to the value of the global variable determined in the Stoke Statup script	if the version is higher than 2018			
			updateLayout()
			--format "LastErrorID=%\n" LastErrorID
			prg_MagmaError.color = if LastErrorID >= 0 then red else green
			updateVoxelGridInfo()
		)
	)
	
	
	rollout display_rollout "Viewport Display"
	(
		checkbox chk_viewportenabled "On" checked:delegate.ViewportEnabled across:3 align:#left offset:[-10,-3]
		spinner spn_viewportReduce "Reduce:" range:[0,999,delegate.ViewportReduce] type:#integer fieldwidth:43 offset:[37,-2]
		button btn_viewportReduce_Preset ">" width:18 height:16 align:#right offset:[10,-2] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Request the reduction of the display samples by showing every Nth.\n\nNote that the system might reduce automatically beyond the requested value to maintain usability."
		
		dotNetControl btn_updateChannels "Button" text:"Update Channels" width:114 height:20 align:#right offset:[11,0] tooltip:"Updates the content of the Mask, Color and Display drop-down lists."
		
		label lbl_maskSource "Mask:" across:2 align:#left offset:[2,1] tooltip:"Set the Mask Source"
		dropdownlist ddl_maskSource items:#("No Mask","Density") align:#right width:114 offset:[11,-3]
		
		label lbl_colorSource "Color:" across:2 align:#left offset:[2,1] tooltip:"Set the Color Source"
		dropdownlist ddl_colorSource items:#("Object Color","Color","Velocity") align:#right width:114 offset:[11,-3]

		label lbl_displayVectorSource "Display:" across:2 align:#left offset:[-7,1]
		dropdownlist ddl_displayVectorSource items:#("Large Dots","Velocity") align:#right width:114 offset:[11,-3]
		
		
		checkbox chk_ViewVectorNormalize "Norm.Length" offset:[-10,-2] across:3 tooltip:"Normalize the Vector channel before displaying it." checked:delegate.ViewportVectorNormalize
		spinner spn_viewVectorScale "" range:[0,1000,delegate.viewportVectorScale] fieldwidth:42 offset:[38,-2] tooltip:"Scale the Vector channel before displaying it." 
		button btn_viewVectorScale_Preset ">" width:18 height:16 align:#right offset:[11,-2] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Scale the Vector channel before displaying it." 


		checkbox chk_ViewportBoundsEnabled "Display Grid Bounds" offset:[-10,2]

		spinner spn_iconSize "Icon Size " range:[0.0,10000.0,delegate.iconSize] fieldwidth:43 across:2 offset:[60,0] tooltip:"Set the size of the STOKE Viewport Icon." type:#worldunits
		button btn_iconSize_Preset ">" width:18 height:16 align:#right offset:[10,0] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the size of the STOKE Viewport Icon."

		group "Manipulator Handles"
		(
			spinner spn_minManipulatorSize "Min. Size " range:[0.0,10000.0,0.1] fieldwidth:43 across:2 offset:[58,0] tooltip:"Set the minimum size of the STOKE Grid Manipulator Handles." type:#worldunits
			button btn_minManipulatorSize_Preset ">" width:18 height:16 align:#right offset:[8,0] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the minimum size of the STOKE Grid Manipulator Handles."
			spinner spn_maxManipulatorSize "Max. Size " range:[0.1,10000.0,10.0] fieldwidth:43 across:2 offset:[58,0] tooltip:"Set the maximum size of the STOKE Grid Manipulator Handles." type:#worldunits
			button btn_maxManipulatorSize_Preset ">" width:18 height:16 align:#right offset:[8,0] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the maximum size of the STOKE Grid Manipulator Handles."
		)

		fn updateUI =
		(
			chk_viewportenabled.state = delegate.ViewportEnabled
			spn_viewportReduce.value = delegate.ViewportReduce
			spn_iconSize.value = delegate.iconSize
			chk_ViewportBoundsEnabled.state = delegate.ViewportBoundsEnabled
			
			spn_viewVectorScale.value = delegate.viewportVectorScale
			
			local theMagma = delegate.MagmaHolder
			--local theChannels = for i in theMagma.GetNodes() where theMagma.GetNodeType i == "Output" and matchPattern (theMagma.getNodeProperty i "channelType") pattern:"float*" collect theMagma.getNodeProperty i "channelName" 
			local theVectors  = for i in theMagma.GetNodes() where theMagma.GetNodeType i == "Output" and matchPattern (theMagma.getNodeProperty i "channelType") pattern:"float*[3]"  collect theMagma.getNodeProperty i "channelName" 
			local theScalars = for i in theMagma.GetNodes() where theMagma.GetNodeType i == "Output" and not matchPattern (theMagma.getNodeProperty i "channelType") pattern:"float*[3]"  collect theMagma.getNodeProperty i "channelName" 

			local maskSources = #("No Mask")
			for i in theScalars do appendIfUnique maskSources i
			ddl_maskSource.items = maskSources
			
			local colorSources = #("Object Color")
			for i in theVectors do appendIfUnique colorSources i
			for i in theScalars do appendIfUnique colorSources i
			ddl_colorSource.items = colorSources
			
			local displayVectors = #("Large Dots")
			for i in theVectors do appendIfUnique displayVectors i
			ddl_displayVectorSource.items = displayVectors
			
			local theIndex = findItem ddl_MaskSource.items delegate.ViewportMaskChannel
			if theIndex == 0 do theIndex = 1
			ddl_MaskSource.selection = theIndex
			
			local theIndex = findItem ddl_ColorSource.items delegate.ViewportColorChannel
			if theIndex == 0 do theIndex = 1
			ddl_ColorSource.selection = theIndex
			
			local theIndex = findItem ddl_displayVectorSource.items delegate.ViewportVectorChannel
			if theIndex == 0 do theIndex = 1
			ddl_displayVectorSource.selection = theIndex
		)		
		
		fn popupPresetMenu type =
		(
			if createPresetsRCMenu type:type do
				popUpMenu StokeField_Presets_RCMenu position:mouse.screenPos		
		)
		on btn_viewportReduce_Preset pressed do popupPresetMenu #viewportreduce
		on btn_viewportReduce_Preset rightclick do popupPresetMenu #viewportreduce

		on btn_iconSize_Preset pressed do popupPresetMenu #iconSize
		on btn_iconSize_Preset rightclick do popupPresetMenu #iconSize
			
		on btn_minManipulatorSize_Preset pressed do popupPresetMenu #minManipulatorSize
		on btn_minManipulatorSize_Preset rightclick do popupPresetMenu #minManipulatorSize

		on btn_maxManipulatorSize_Preset pressed do popupPresetMenu #maxManipulatorSize
		on btn_maxManipulatorSize_Preset rightclick do popupPresetMenu #maxManipulatorSize

		on btn_viewVectorScale_Preset pressed do  popupPresetMenu #viewVectorScale
		on btn_viewVectorScale_Preset rightclick do  popupPresetMenu #viewVectorScale
			
		on btn_updateChannels click arg do updateUI()
		
		on chk_ViewportBoundsEnabled  changed state do 
		(
			delegate.ViewportBoundsEnabled = state
		)		
		
		on chk_viewportenabled changed state do 
		(
			delegate.ViewportEnabled = state
			redrawViews()
		)
		
		on ddl_maskSource selected itm do 
		(
			delegate.ViewportMaskChannel = if itm == 1 then "" else ddl_maskSource.selected
			redrawViews()
		)
		on ddl_colorSource selected itm do 
		(
			delegate.ViewportColorChannel = if itm == 1 then "" else ddl_colorSource.selected
			redrawViews()
		)
		on ddl_displayVectorSource selected itm do 
		(
			delegate.ViewportVectorChannel = if itm == 1 then "" else ddl_displayVectorSource.selected
			redrawViews()
		)
		
		on spn_viewVectorScale changed val do 
		(
			delegate.viewportVectorScale = val
			redrawviews()
		)
		on chk_ViewVectorNormalize changed state do 
		(
			delegate.ViewportVectorNormalize = state		
			redrawviews()
		)
		
		
		on spn_iconSize changed val do
		(
			delegate.iconSize = val
			redrawViews()			
		)
		on spn_viewportReduce changed val do 
		(
			delegate.ViewportReduce = val
			redrawViews()	
		)
		
		fn updateLayout =
		(
			btn_updateChannels.FlatStyle = btn_updateChannels.FlatStyle.System
			
			btn_updateChannels.width = ddl_maskSource.width = ddl_colorSource.width = ddl_displayVectorSource.width = display_rollout.width-53
			btn_updateChannels.pos.x = ddl_maskSource.pos.x= ddl_colorSource.pos.x = ddl_displayVectorSource.pos.x = 50
		
			spn_viewportReduce.pos.x = spn_viewVectorScale.pos.x = spn_iconSize.pos.x = display_rollout.width-33
			spn_minManipulatorSize.pos.x = spn_maxManipulatorSize.pos.x = display_rollout.width-35
			
			chk_ViewVectorNormalize.text = if display_rollout.width > 200 then "Normalize Length" else "Norm.Length"
			chk_ViewVectorNormalize.pos.x = 2
		)
		
		on display_rollout open do 
		(
			if (maxVersion())[1] > 20000 do display_rollout.autoLayoutOnResize = ::Stoke_autoLayoutOnResizeOn	--set to the value of the global variable determined in the Stoke Statup script	if the version is higher than 2018			
			updateLayout()
			updateUI()
		)
			
	)
	
	tool create 
	( 
		local theObj, startpoint, endpoint 
		on mousePoint clickno do
		(
			case clickno of
			(
				1:
				(
					nodeTM.translation = startpoint = gridPoint
					BoundsMinX = gridPoint.x
					BoundsMinY = gridPoint.y
					BoundsMinZ = gridPoint.z	
					pushprompt "HOLD DOWN Left Mouse Button and MOVE the Mouse to DEFINE Grid Base Extents. RELEASE LMB to DEFINE the Grid Height..."
				)
				2:
				(
					endpoint = gridPoint
					popprompt()
					pushprompt "LEFT-CLICK to Set the HEIGHT and FINISH Grid creation. RIGHT-CLICK to CANCEL creation..."
				)
				3: 
				(
					popprompt()
					pushprompt "LEFT-CLICK again to CREATE ANOTHER Field Magma object. RIGHT-CLICK to EXIT Field Magma creation mode."
					#stop
				)
			)
		)
		on mouseMove clickno do
		(
			case clickno of
			(
				2:
				(
					if gridPoint.x >= BoundsMinX+GridSpacing*3 then 
					(
						BoundsMaxX = gridPoint.x 
						BoundsMinX = startPoint.x
					)
					else 
					(
						BoundsMinX = gridPoint.x 
						BoundsMaxX = startPoint.x
					)
					if gridPoint.y >= BoundsMinY+GridSpacing*3 then 
					(
						BoundsMaxY = gridPoint.Y 
						BoundsMinY = startPoint.y
					)
					else 
					(
						BoundsMinY = gridPoint.y
						BoundsMaxY = startPoint.y
					)
					BoundsMaxZ = gridPoint.z + GridSpacing*2
					local theBox = [boundsMinX,boundsMinY,boundsMinZ] + (([boundsMaxX,boundsMaxY,boundsMaxZ]-[boundsMinX,boundsMinY,boundsMinZ])/2)
					nodeTM.translation = [theBox.x,theBox.y, boundsMinZ]
					local theBBoxSize = [BoundsmaxX,BoundsmaxY,BoundsmaxZ] - [BoundsminX,BoundsminY, BoundsminZ]
					GridSpacing = (length theBBoxSize)/50.0							
				)
				3:
				(
					BoundsMaxZ = amax #(0.5*length (endpoint-gridPoint), gridPoint.z+GridSpacing*2)
					local theBBoxSize = [BoundsmaxX,BoundsmaxY,BoundsmaxZ] - [BoundsminX,BoundsminY, BoundsminZ]
					GridSpacing = (length theBBoxSize)/50.0							
				)
			)
		)	
	) 	

	fn onMagmaError msg NodeID:unsupplied ExpressionID:unsupplied = 
	(
		local existingEditor = (for i in ::MagmaFlowEditor_CurrentEditors where i != undefined and i[1] == delegate.MagmaHolder collect i)
		if msg != undefined then
		(
			if ExpressionID != unsupplied do 
			(
				--print "Error!"
				params_rollout.prg_MagmaError.color = red
				LastErrorID = nodeID
				LastErrorMsg = msg
				if existingEditor.count == 1 do
					existingEditor[1][2].updateErrorInfo NodeID:NodeID ErrorMessage:msg 
			)
		)
		else
		(
			--print "No Error"
			LastErrorID = -100
			params_rollout.prg_MagmaError.color = green
			if existingEditor.count == 1 do
				existingEditor[1][2].updateErrorInfo NodeID:-100 ErrorMessage:""
		)
	)
	
	on create do
	(
		manipulateMode = true
		delegate.SetCallback onMagmaError
		delegate.ViewportMaskChannel = "Density"
		delegate.ViewportBoundsEnabled = true
		delegate.magmaHolder.autoUpdate = true
		
		magmaNode = delegate.magmaHolder
				
		node0 = magmaNode.createNode "Output" 
		magmaNode.setNumNodeInputs node0 1 
		magmaNode.setNumNodeOutputs node0 0 
		magmaNode.setNodeProperty node0 "channelName" "Velocity"
		magmaNode.setNodeProperty node0 "channelType" "float32[3]"
		magmaNode.DeclareExtensionProperty node0 "Position"
		magmaNode.SetNodeProperty node0 "Position" [644,212]
		--------------------------------------------
		/*node1 = magmaNode.createNode "InputValue" 
		magmaNode.setNumNodeInputs node1 0 
		magmaNode.setNumNodeOutputs node1 1 
		magmaNode.setNodeProperty node1 "forceInteger" false
		ctrl=Point3_XYZ(); ctrl.value = [0,0,100]
		magmaNode.setNodeProperty node1 "controller" ctrl
		magmaNode.DeclareExtensionProperty node1 "Position"
		magmaNode.SetNodeProperty node1 "Position" [551,0]*/
		--------------------------------------------
		node2 = magmaNode.createNode "Output" 
		magmaNode.setNumNodeInputs node2 1 
		magmaNode.setNumNodeOutputs node2 0 
		magmaNode.setNodeProperty node2 "channelName" "Density"
		magmaNode.setNodeProperty node2 "channelType" "float32"
		magmaNode.DeclareExtensionProperty node2 "Position"
		magmaNode.SetNodeProperty node2 "Position" [570,181]
		--------------------------------------------
		/*node3 = magmaNode.createNode "InputValue" 
		magmaNode.setNumNodeInputs node3 0 
		magmaNode.setNumNodeOutputs node3 1 
		magmaNode.setNodeProperty node3 "forceInteger" false
		ctrl=bezier_float(); ctrl.value = 0.0
		magmaNode.setNodeProperty node3 "controller" ctrl
		magmaNode.DeclareExtensionProperty node3 "Position"
		magmaNode.SetNodeProperty node3 "Position" [551,55]*/
		--------------------------------------------
		--try(magmaNode.setNodeInput node0 1 node1 1)catch()
		--try(magmaNode.setNodeInput node2 1 node3 1)catch()
		--------------------------------------------
		
		local theVal=getIniSetting (getDir #plugcfg + "//StokePreferences.ini") "Log" "Level"
		if theVal != "" do try(StokeGlobalInterface.LoggingLevel = theVal as name)catch()		
	)

	on Load do 
	( 
		delegate.SetCallback onMagmaError
	)	
	
	on clone oldObj do 
	( 
		
	)
	
)


plugin Geometry StokeFieldLoader
name:"Field Loader"
category:"Stoke"
classid:#(0x17aaa029, 0x23722adb)
extends: Stoke_Field_Loader_Base
replaceui:true
(

	local display_rollout
	
	fn createPresetsRCMenu type:#renderpercent =
	(
		if selection[1] == undefined or classof selection[1].baseobject != StokeFieldLoader do return false
		local isTime = false
		local isInDelegate = false
		case type of
		(
			#gridspacing: (
						presetName = "gridSpacing"
						theParameter = "gridSpacing"
						isTime = false
			)
			#viewportreduce: (
						presetName = "viewportreduce"
						theParameter = "viewportreduce"
						isTime = false
						isInDelegate = true
			)
			#iconsize: (
						presetName = "iconSize"
						theParameter = "iconSize"
						isTime = false
						isInDelegate = true
			)		
			#viewVectorScale: (
						presetName = "viewVectorScale"
						theParameter = "ViewportVectorScale"
						isTime = false
						isInDelegate = true
			)
			#cacheCapacityMB: (
						presetName = "cacheCapacityMB"
						theParameter = "cacheCapacityMB"
						isTime = false
						isInDelegate = true				
			)			
		)
		local presetsList = #()
		local theKeys = for i in (getIniSetting (getDir #plugcfg + "//StokeFieldSimPreferences.ini") presetName  ) collect (execute i)
		sort theKeys 
		if theKeys.count == 0 then 
		(
			theKeys = case type of
			(
				#gridspacing: #(1.0,2.0,3.0,5.0,8.0,10.0,20.0,50.0)
				#viewportreduce: #(1,2,3,5,10,20,50,100)
				#iconSize: #(1.0,10.0,30.0,50.0,100.0)
				#viewVectorScale: #(0.01,0.03,0.04,0.05,0.1,0.3,0.5,1.0,2.0,10.0,24.0,25.0,30.0)
				#cacheCapacityMB: #(64,128,256,512,1024,2048)
			)
			for i in theKeys do
				setIniSetting (getDir #plugcfg + "//StokeFieldSimPreferences.ini") presetName (i as string) (i as string)
		)
		for k in theKeys do
			if findItem presetsList theValue == 0 do append presetsList k

		local theDelText = if isInDelegate then "delegate." else ""
		theValue = execute ("selection[1]." + theDelText + theParameter)
		global StokeField_Presets_RCMenu
		local txt = "rcmenu StokeField_Presets_RCMenu\n(\n"
		
		txt+= "fn updateUI = (\n"
		txt+= "try(selection[1].params_Rollout.updateUI())catch()\n"
		txt+= "try(selection[1].display_rollout.updateUI())catch()\n"
		
		txt+= ")\n"
		
		if isTime do
		(
			txt += "menuItem mnu_StartFrame \""+ (animationrange.start.frame as integer) as string+" - Start Frame \"\n" 
			txt += "on mnu_StartFrame picked do (selection[1]."+  theParameter +" = "+ (animationrange.start.frame as integer) as string +"\nupdateUI())\n"

			txt += "menuItem mnu_CurrentFrame \""+ (sliderTime.frame as integer) as string+" - Current Frame\"\n" 
			txt += "on mnu_CurrentFrame picked do (selection[1]."+ theParameter +" = "+ (sliderTime.frame as integer) as string +"\nupdateUI())\n"

			txt += "menuItem mnu_EndFrame \""+ (animationrange.end.frame as integer) as string+" - End Frame\"\n" 
			txt += "on mnu_EndFrame picked do (\nselection[1]."+ theParameter +" = "+ (animationrange.end.frame as integer) as string +"\nupdateUI())\n"
			
			txt += "separator spr_10\n"
		)	
		if type == #gridspacing do
		(
			local theDefaultValue = (length ([boundsMaxX,boundsMaxY,boundsMaxZ ]-[boundsMinX,boundsMinY,boundsMinZ])) / 50.0
			txt += "menuItem mnu_defaultSpacing \"Default "+ theDefaultValue as string+"\"\n" 
			txt += "on mnu_defaultSpacing picked do (\nselection[1]."+ theParameter +" = "+ theDefaultValue as string +"\nupdateUI())\n"
			txt += "separator spr_15\n" 			
		)
			
		if findItem presetsList theValue == 0 do 
		(
			txt += "menuItem mnu_AddPreset \"Add "+ theValue as string+"\"\n" 
			txt += "on mnu_AddPreset picked do (setIniSetting (getDir #plugcfg + \"//StokeFieldSimPreferences.ini\") \""+ presetName + "\" \""+ theValue as string +"\" \""+ theValue as string +"\" \n updateUI())\n"
			txt += "separator spr_20\n" 
		)	
		
		cnt = 0
		for i in presetsList do
		(
			cnt += 1
			txt += "menuItem mnu_preset"+ cnt as string +" \"" + i as string + "\" \n" 
			txt += "on mnu_preset" + cnt as string + " picked do ( selection[1]."+ theDelText + theParameter +" = "+ i as string +"\n"
			txt += "updateUI()\n" 
			txt += ")\n"
		)
		
		if findItem presetsList theValue != 0 do 
		(
			txt += "separator spr_100\n" 
			txt += "menuItem mnu_RemovePreset \"Remove "+ theValue as string+"\"\n" 
			txt += "on mnu_RemovePreset picked do delIniSetting (getDir #plugcfg + \"//StokeFieldSimPreferences.ini\") \""+ presetName +"\" \""+ theValue as string +"\" \n"
		)	
		txt += ")\n"
		execute txt
		true
	)		
	
	fn playbackgraph_currentSegment mode =
	(
		try(deleteKeys selection[1].delegate.PlaybackTime.controller #allKeys)catch()
		local theCtrl = selection[1].delegate.PlaybackTime.controller = selection[1].PlaybackTime.controller = bezier_float()
		
		case mode of
		(
			#linear:
			(
				theKey = addNewKey theCtrl animationrange.start
				theKey.value = animationrange.start.frame
				theKey.inTangentType = theKey.outTangentType = #linear
				
				theKey = addNewKey theCtrl animationrange.end
				theKey.value = animationrange.end.frame
				theKey.inTangentType = theKey.outTangentType = #linear
				
				setBeforeORT theCtrl #linear
				setAfterORT theCtrl #linear
			)	
			#accel:
			(
				theKey = addNewKey theCtrl animationrange.start
				theKey.value = animationrange.start.frame
				theKey.inTangentType = theKey.outTangentType = #slow
				
				theKey = addNewKey theCtrl animationrange.end
				theKey.value = animationrange.end.frame
				theKey.inTangentType = theKey.outTangentType = #fast
				
				setBeforeORT theCtrl #linear
				setAfterORT theCtrl #linear
			)
			#decel:
			(
				theKey = addNewKey theCtrl animationrange.start
				theKey.value = animationrange.start.frame
				theKey.inTangentType = theKey.outTangentType = #fast
				
				theKey = addNewKey theCtrl animationrange.end
				theKey.value = animationrange.end.frame
				theKey.inTangentType = theKey.outTangentType = #slow
				
				setBeforeORT theCtrl #linear
				setAfterORT theCtrl #linear
			)		

			#pingpong:
			(
				theKey = addNewKey theCtrl animationrange.start
				theKey.value = animationrange.start.frame
				theKey.inTangentType = theKey.outTangentType = #auto
				
				theKey = addNewKey theCtrl (animationrange.start.frame + ((animationrange.end.frame - animationrange.start.frame)/2))
				theKey.value = animationrange.end.frame
				theKey.inTangentType = theKey.outTangentType = #auto
				
				theKey = addNewKey theCtrl animationrange.end
				theKey.value = animationrange.start.frame
				theKey.inTangentType = theKey.outTangentType = #auto
				
				setBeforeORT theCtrl #linear
				setAfterORT theCtrl #linear
			)	
		)
	)

	fn playbackgraph_invertAnimation =
	(
		local theCtrl = try(selection[1].PlaybackTime.controller = selection[1].delegate.PlaybackTime.controller)catch(undefined)
		if theCtrl != undefined do
		(
			try(reverseTime theCtrl theCtrl.keys[1].time theCtrl.keys[theCtrl.keys.count].time  #incLeft #incRight)catch()
		)	
	)	

	fn playbackgraph_deleteAnimation = 
	(
		local theCtrl = try(selection[1].PlaybackTime.controller = selection[1].delegate.PlaybackTime.controller)catch(undefined)
		if theCtrl != undefined do
			try(deleteKeys theCtrl #allKeys)catch()
	)	
	
	fn createKeyframeToolsMenu =
	(
		if selection[1] == undefined or classof selection[1].baseobject != StokeFieldLoader do return undefined
		rcmenu SFL_KeyframeToolsRCMenu
		(
			menuitem mnu_playbackgraph_currentSegment_Linear "Create LINEAR Playback Keys"
			menuitem mnu_playbackgraph_currentSegment_Accelerate "Create ACCELERATION Playback Keys"
			menuitem mnu_playbackgraph_currentSegment_Decelerate "Create DECELERATION Playback Keys"
			menuitem mnu_playbackgraph_currentSegment_PingPong "Create PING-PONG Playback Keys"
			separator sep_10 
			subMenu "Out-Of-Range Types"
			(
				menuitem mnu_playbackgraph_ORT_Before_Constant "Before CONSTANT" checked:(try(getBeforeORT selection[1].PlaybackTime.controller == #constant)catch(false))
				menuitem mnu_playbackgraph_ORT_Before_Cycle "Before CYCLE" checked:(try(getBeforeORT selection[1].PlaybackTime.controller == #cycle)catch(false))
				menuitem mnu_playbackgraph_ORT_Before_Loop "Before LOOP" checked:(try(getBeforeORT selection[1].PlaybackTime.controller == #loop)catch(false))
				menuitem mnu_playbackgraph_ORT_Before_PingPong "Before PING PONG" checked:(try(getBeforeORT selection[1].PlaybackTime.controller == #pingpong)catch(false))
				menuitem mnu_playbackgraph_ORT_Before_Linear "Before LINEAR" checked:(try(getBeforeORT selection[1].PlaybackTime.controller == #linear)catch(false))
				menuitem mnu_playbackgraph_ORT_Before_Repeat "Before RELATIVE REPEAT" checked:(try(getBeforeORT selection[1].PlaybackTime.controller == #relativerepeat)catch(false))
				separator sep_100
				menuitem mnu_playbackgraph_ORT_After_Constant "After CONSTANT" checked:(try(getAfterORT selection[1].PlaybackTime.controller == #constant)catch(false))
				menuitem mnu_playbackgraph_ORT_After_Cycle "After CYCLE" checked:(try(getAfterORT selection[1].PlaybackTime.controller == #cycle)catch(false))
				menuitem mnu_playbackgraph_ORT_After_Loop "After LOOP" checked:(try(getAfterORT selection[1].PlaybackTime.controller == #loop)catch(false))
				menuitem mnu_playbackgraph_ORT_After_PingPong "After PING PONG" checked:(try(getAfterORT selection[1].PlaybackTime.controller == #pingpong)catch(false))
				menuitem mnu_playbackgraph_ORT_After_Linear "After LINEAR" checked:(try(getAfterORT selection[1].PlaybackTime.controller == #linear)catch(false))
				menuitem mnu_playbackgraph_ORT_After_Repeat "After RELATIVE REPEAT" checked:(try(getAfterORT selection[1].PlaybackTime.controller == #relativerepeat)catch(false))
			)
			separator sep_20 
			menuitem mnu_playbackgraph_invertAnimation "INVERT Existing Animation"
			separator sep_30 
			menuitem mnu_playbackgraph_deleteAnimation "DELETE Existing Animation"
			
			on mnu_playbackgraph_ORT_Before_Constant picked do
				try(setBeforeORT selection[1].PlaybackTime.controller #constant)catch()
			on mnu_playbackgraph_ORT_Before_Cycle picked do
				try(setBeforeORT selection[1].PlaybackTime.controller #cycle)catch()
			on mnu_playbackgraph_ORT_Before_Loop picked do
				try(setBeforeORT selection[1].PlaybackTime.controller #loop)catch()
			on mnu_playbackgraph_ORT_Before_PingPong picked do
				try(setBeforeORT selection[1].PlaybackTime.controller #pingpong)catch()
			on mnu_playbackgraph_ORT_Before_Linear picked do
				try(setBeforeORT selection[1].PlaybackTime.controller #linear)catch()
			on mnu_playbackgraph_ORT_Before_Repeat picked do
				try(setBeforeORT selection[1].PlaybackTime.controller #relativerepeat)catch()
			
			on mnu_playbackgraph_ORT_After_Constant picked do
				try(setAfterORT selection[1].PlaybackTime.controller #constant)catch()
			on mnu_playbackgraph_ORT_After_Cycle picked do
				try(setAfterORT selection[1].PlaybackTime.controller #cycle)catch()
			on mnu_playbackgraph_ORT_After_Loop picked do
				try(setAfterORT selection[1].PlaybackTime.controller #loop)catch()
			on mnu_playbackgraph_ORT_After_PingPong picked do
				try(setAfterORT selection[1].PlaybackTime.controller #pingpong)catch()
			on mnu_playbackgraph_ORT_After_Linear picked do
				try(setAfterORT selection[1].PlaybackTime.controller #linear)catch()
			on mnu_playbackgraph_ORT_After_Repeat picked do
				try(setAfterORT selection[1].PlaybackTime.controller #relativerepeat)catch()				
			
			on mnu_playbackgraph_currentSegment_Linear picked do 
			(
				playbackgraph_currentSegment #linear
			)
			on mnu_playbackgraph_currentSegment_Accelerate picked do
			(
				playbackgraph_currentSegment #accel
			)
			on mnu_playbackgraph_currentSegment_Decelerate picked do
			(
				playbackgraph_currentSegment #decel
			)
			on mnu_playbackgraph_currentSegment_PingPong picked do 
			(
				playbackgraph_currentSegment #pingpong
			)
			
			on mnu_playbackgraph_invertAnimation picked do
			(					
				playbackgraph_invertAnimation()
			)
			on mnu_playbackgraph_deleteAnimation picked do 
			(
				playbackgraph_deleteAnimation()
			)
		)
		
		SFL_KeyframeToolsRCMenu
	)
	
	parameters params rollout:params_rollout
	(
		filenamePattern type:#string default:"" ui:txt_filenamePattern
		PlaybackTime type:#float default:0 ui:spn_PlaybackTime animatable:true
		--channelsList type:#stringtab default:"" tabSizeVariable:true
		
		on filenamePattern set val do delegate.filenamePattern = val
		on PlaybackTime set val do delegate.PlaybackTime = val
	)
	
	fn createFileUtilsMenu =
	(
		rcmenu fileUtils 
		(
			menuItem mnu_openFDV "OPEN Stoke Field Data VIEWER..."
			--separator sep_10
			menuItem mnu_openFDP "OPEN Stoke Field Data PROBE..."
			menuItem mnu_openFDE "OPEN Stoke Field Data EXPORTER..."


			separator sep_20
			menuItem mnu_addDisplayModifier "ADD Stoke DISPLAY Modifier"
			
			separator sep_100
			menuItem mnu_exploreFolder "EXPLORE Sequence Folder..."
			
			on mnu_openFDV picked do 
			(
				try(fileIn (StokeGlobalInterface.HomeDirectory+"\\Scripts\\Stoke_FieldDataViewer.ms"))catch()
			)
			
			on mnu_openFDP picked do
			(
				Macros.run "Stoke" "StokeFieldProbe" 
			)
			
			on mnu_openFDE picked do
			(
				Macros.run "Stoke" "StokeFieldDataExporter" 
			)		

			on mnu_addDisplayModifier picked do
			(
				modPanel.addModToSelection (Stoke_Field_Display_Modifier())
			)
			
			on mnu_exploreFolder  picked do
			(
				shellLaunch (getFileNamePath $.filenamePattern) ""
			)
		)
		fileUtils
	)	
	
	fn createDependentsMenu =
	(
		local theDeps = for o in refs.dependents this where isKindOf o Node and isProperty o #pos and classof o.baseobject != StokeFieldLoader collect o	
		local txt = "" as stringStream
		format "rcmenu selectDependentNodes_Menu (\n" to:txt
		if theDeps.count == 0 then
		(
			format "menuItem mnu_item0 \"--No Dependent Objects Found!--\"\n" to:txt
		)
		else
		(
			for i = 1 to theDeps.count do
			(
				format "menuItem mnu_item% \"Select '%' (%)\"\n" i theDeps[i].name (classof theDeps[i]) to:txt
				format "on mnu_item% picked do select (getNodeByName \"%\") \n" i theDeps[i].name to:txt
			)
		)
		format ")\n" to:txt
		execute (txt as string)
	)
	
	rollout params_rollout "Stoke Field Loader"
	(
		dotNetControl btn_getFilename "Button" text:"Pick A File Or Sequence..." width:138 offset:[0,-3] height:22 align:#left across:2
		button btn_fileUtilities ">>" width:18 offset:[6,-3]  images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) height:22 align:#right
		edittext txt_filenamePattern fieldwidth:156 pos:[-1,27]
		dotNetControl dnc_fileInfo "ListView" width:156 align:#center height:150 offset:[0,-4]
		
		dotNetControl btn_selectDependentNodes "Button" text:"Select Dependent Object..." align:#center width:156 height:22
		checkbox chk_InWorldSpace "Apply Object Transforms" checked:(not delegate.InWorldSpace) offset:[-6,-1] align:#left tooltip:"When checked, the data will be transformed into World Space.\n\nWhen unchecked, the data will be loaded as saved, ignoring the Field Loader's Transforms."
		
		on btn_selectDependentNodes click arg do
		(
			local theMenu = createDependentsMenu()
			popUpMenu theMenu pos:mouse.screenpos
		)
		
		on btn_fileUtilities pressed do
		(
			local fileUtils = createFileUtilsMenu()
			popupMenu fileUtils pos:mouse.screenpos
		)
		
		group "Timing"
		(
			checkbox chk_SubstituteSequenceNumber "Load Single Frame Only" checked:(not delegate.SubstituteSequenceNumber) offset:[-2,-3]
			checkbox chk_UsePlaybackTime "Graph [a]:" across:2 align:#left checked:(delegate.UsePlaybackTime) offset:[-2,-1]
			spinner spn_PlaybackTime "" range:[-1000000,1000000,0]  align:#right fieldwidth:42 offset:[-12,0]
			label lbl_offset "Offset:  " across:2 align:#left offset:[12,-2]
			spinner spn_PlaybackOffset range:[-100000,100000,delegate.PlaybackOffset] type:#integer align:#right fieldwidth:42  offset:[-12,-3]
			button btn_keyframeTools ">>" height:36 width:18 offset:[7,-40] align:#right images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2)
			checkbox chk_UsePlaybackRange "Limit To Custom Range:" offset:[-2,-3] checked:(delegate.UsePlaybackRange)
			dotNetControl btn_range "Button" text:"Range" height:18 width:40 align:#left across:3 offset:[-3,-1] tooltip:"Set the Start and End Frames of the Custom Range according to the first and last frames of the current sequence."
			spinner spn_PlaybackRangeStart type:#integer fieldwidth:37 align:#center offset:[-8,0] range:[-100000,100000,delegate.PlaybackRangeStart]
			spinner spn_PlaybackRangeEnd "-" type:#integer fieldwidth:37 align:#right offset:[3,0] range:[-100000,100000,delegate.PlaybackRangeEnd]
			dropdownlist ddl_PlaybackRangeStartORT items:#("Hold First","Blank") selection:(delegate.PlaybackRangeStartORT+1) across:2 width:70 align:#left offset:[-3,0]
			dropdownlist ddl_PlaybackRangeEndORT items:#("Hold Last","Blank") selection:(delegate.PlaybackRangeEndORT+1) width:70 align:#right offset:[3,0]
		)
		group "Memory"
		(
			spinner spn_cacheCapacityMB "Cache in MB:" type:#integer range:[0,100000,delegate.cacheCapacityMB] fieldwidth:43 across:2 offset:[57,0] 
			button btn_cacheCapacityMB_Preset ">" width:18 height:16 align:#right offset:[7,0] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the size of the Field Loader's Cache in Megabytes."
		)

		fn addCommas txt =
		(
			if matchPattern txt pattern:"*L" do txt = substring txt 1 (txt.count-1)
			if matchPattern txt pattern:"*P" do txt = substring txt 1 (txt.count-1)
			theDot = findString txt "."
			if theDot == undefined then 
			(
				newTxt = "" 
				
				theDot = txt.count
			)	
			else 
			(
				newTxt = substring txt theDot -1
				theDot -= 1
			)
			cnt = 0
			for i = theDot to 1 by -1 do
			(
				cnt +=1
				newTxt = txt[i] + newTxt
				if cnt == 3 and i > 1 and txt[i-1] != "-" do 
				(
					newTxt = "," + newTxt
					cnt = 0
				)	
			)
			newTxt
		)
		
		fn initListView =
		(
			local lv = dnc_fileInfo
			layout_def = #( #("Data",45), #("Value",150) )
			lv.Clear()
			local maxBgColor = (((colorman.getcolor #window)) as color)*255
			local dncBGColor = (dotNetClass "System.Drawing.Color").fromARGB maxBgColor.r maxBgColor.g maxBgColor.b
			
			lv.backColor = dncBGColor
			lv.View = (dotNetClass "System.Windows.Forms.View").Details
			lv.gridLines = true 
			lv.fullRowSelect = true
			lv.checkboxes = false
			lv.hideSelection = false
			lv.ShowItemToolTips = true
			for i in layout_def do lv.Columns.add i[1] i[2]			
		)
		
		fn updateUI = 
		(
			spn_PlaybackTime.enabled = spn_PlaybackOffset.enabled = chk_UsePlaybackRange.enabled = btn_range.enabled = spn_PlaybackRangeStart.enabled = spn_PlaybackRangeEnd.enabled = ddl_PlaybackRangeStartORT.enabled = ddl_PlaybackRangeEndORT.enabled = delegate.SubstituteSequenceNumber
			spn_cacheCapacityMB.value = delegate.cacheCapacityMB
			
			local theInterface = try(StokeGlobalInterface.CreateMXSField selection[1])catch(undefined)
			if theInterface != undefined do
			(
				local theBox = theInterface.BoundsMax-theInterface.BoundsMin
				local theGrid = theBox / theInterface.Spacing
				local theCount = (theGrid.x as integer64 * theGrid.y as integer64 * theGrid.z as integer64) as string
				theCount = substring theCount 1 (theCount.count - 1)
				
				local textColor = ( ((colorman.getcolor #text) as color)*255)
				local dncTextColor = (dotNetClass "System.Drawing.Color").fromARGB textColor.r textColor.g textColor.b

				local theRange = #()
				local lv = dnc_fileInfo
				lv.items.Clear()
				local li = dotNetObject "System.Windows.Forms.ListViewItem" "Grid"
				li.forecolor = dncTextColor
				local subLi = li.SubItems.add ((theGrid.x as integer) as string + "x" + (theGrid.y as integer) as string + "x" + (theGrid.z as integer) as string)
				append theRange li
				
				local li = dotNetObject "System.Windows.Forms.ListViewItem" "Count"
				li.forecolor = dncTextColor
				local subLi = li.SubItems.add (addCommas theCount)
				append theRange li
				
				local li = dotNetObject "System.Windows.Forms.ListViewItem" "BBox"
				li.forecolor = dncTextColor
				local subLi = li.SubItems.add (theBox as string)
				append theRange li
				
				local li = dotNetObject "System.Windows.Forms.ListViewItem" "Spacing"
				li.forecolor = dncTextColor
				local subLi = li.SubItems.add (theInterface.Spacing as string)
				append theRange li
				
				for c = 1 to theInterface.Channels.count do
				(
					local li = dotNetObject "System.Windows.Forms.ListViewItem" ("Ch."+c as string)
					li.forecolor = dncTextColor
					local subLi = li.SubItems.add (theInterface.Channels[c] as string)
					append theRange li
				)

				lv.Items.AddRange theRange		

				theInterface.Clear()				
			)
		)
		
		fn popupPresetMenu type =
		(
			if createPresetsRCMenu type:type do
				popUpMenu StokeField_Presets_RCMenu position:mouse.screenPos		
		)
		on btn_cacheCapacityMB_Preset pressed do popupPresetMenu #cacheCapacityMB
		on btn_cacheCapacityMB_Preset rightclick do popupPresetMenu #cacheCapacityMB		
			
		on btn_keyframeTools pressed do 
		( 
			local result = createKeyframeToolsMenu() 
			if result != undefined do popupMenu result position:mouse.screenPos		
		)
		on btn_keyframeTools rightclick do 
		( 
			local result = createKeyframeToolsMenu() 
			if result != undefined do popupMenu result position:mouse.screenPos		
		)	
			
		on spn_iconSize changed val do delegate.iconsize = val
		on spn_ViewportReduce changed val do delegate.ViewportReduce = val
		on chk_SubstituteSequenceNumber changed val do 
		(
			delegate.SubstituteSequenceNumber = not val
			updateUI()
		)
		on chk_InWorldSpace changed state do
		(
			delegate.InWorldSpace = not state
			updateUI()
		)
		on spn_PlaybackOffset changed val do delegate.PlaybackOffset = val
		on chk_UsePlaybackTime changed val do delegate.UsePlaybackTime = val
		
		on chk_UsePlaybackRange changed val do delegate.UsePlaybackRange = val
		on spn_PlaybackRangeStart changed val do 
		(
			if val <= delegate.PlaybackRangeEnd then
				delegate.PlaybackRangeStart = val
			else
				delegate.PlaybackRangeStart = spn_PlaybackRangeStart.value = delegate.PlaybackRangeEnd
		)
		on spn_PlaybackRangeEnd changed val do 
		(
			if val >= delegate.PlaybackRangeStart then
				delegate.PlaybackRangeEnd = val
			else
				delegate.PlaybackRangeEnd = spn_PlaybackRangeEnd.value =  delegate.PlaybackRangeStart 
		)
		on spn_cacheCapacityMB changed val do delegate.cacheCapacityMB = val
			
		on ddl_PlaybackRangeStartORT selected itm do delegate.PlaybackRangeStartORT = itm-1
		on ddl_PlaybackRangeEndORT selected itm do delegate.PlaybackRangeEndORT = itm-1
			
		on btn_range click arg do
		(
			local thePath = txt_filenamePattern.text
			if doesFileExist thePath do
			(
				local thePathPattern = getFileNamePath thePath + (substring (getFileNameFile thePath) 1 ((getFileNameFile thePath).count-4)) + "*" + getFilenameType thePath
				local theFiles = sort (getFiles thePathPattern )
				if theFiles.count > 0 do
				(
					local theStart = execute (substring (getFileNameFile theFiles[1]) ((getFileNameFile theFiles[1]).count-3) 4)
					local theEnd = execute (substring (getFileNameFile theFiles[theFiles.count]) ((getFileNameFile theFiles[theFiles.count]).count-3) 4)
					if classof theStart == Integer do spn_PlaybackRangeStart.value = delegate.PlaybackRangeStart = theStart
					if classof theEnd == Integer do spn_PlaybackRangeEnd.value = delegate.PlaybackRangeEnd = theEnd
				)
			)
		)
		
		on btn_getFilename click arg do
		(
			local theFile = getOpenFileName types:"All Supported Formats (*.vdb, *.f3d, *.fxd)|*.vdb;*.f3d;*.fxd|OpenVDB (*.vdb)|*.vdb|Field3D (*.f3d)|*.f3d|FumeFX (*.fxd)|*.fxd"
			if theFile != undefined do 
			(
				local theFileName = getFileNameFile theFile
				local lastDigits = substring theFileName (theFileName.count-3) 4
				delegate.SubstituteSequenceNumber = isKindOf (execute lastDigits) Integer
				chk_SubstituteSequenceNumber.checked = not delegate.SubstituteSequenceNumber
				
				filenamePattern = theFile
				display_rollout.updateUI()
				updateUI()
				
				/*
				local theNode = refs.dependentNodes this
				if theNode.count > 0 do
				(
					local theField = StokeGlobalInterface.CreateMXSField theNode[1]
					if theField != undefined do 
						channelsList = theField.Channels
					theField = undefined
				)
				*/
			)
		)
		
		fn updateLayout =
		(
			btn_range.FlatStyle = btn_selectDependentNodes.FlatStyle = btn_getFilename.FlatStyle = btn_getFilename.FlatStyle.System
			btn_selectDependentNodes.width = dnc_fileInfo.width = txt_filenamePattern.width = params_rollout.width - 14
			btn_getFilename.width = params_rollout.width - 32
			btn_selectDependentNodes.pos.x = dnc_fileInfo.pos.x = txt_filenamePattern.pos.x = btn_getFilename.pos.x = 7
			btn_range.width = params_rollout.width - 122
			spn_PlaybackRangeStart.pos.x = params_rollout.width - 75
			ddl_PlaybackRangeStartORT.width = ddl_PlaybackRangeEndORT.width = (params_rollout.width - 20)/2
			ddl_PlaybackRangeStartORT.pos.x = 10
			ddl_PlaybackRangeEndORT.pos.x = 10 + ddl_PlaybackRangeStartORT.width + 1
			spn_cacheCapacityMB.pos.x = params_rollout.width - 36
		)
		
		on params_rollout open do
		(
			if (maxVersion())[1] > 20000 do params_rollout.autoLayoutOnResize = ::Stoke_autoLayoutOnResizeOn	--set to the value of the global variable determined in the Stoke Statup script	if the version is higher than 2018			
			updateLayout()
			initListView()
			updateUI()
		)
	)
	
	rollout display_rollout "Viewport Display"
	(
		checkbox chk_viewportenabled "On" checked:delegate.ViewportEnabled across:3 align:#left offset:[-10,-3]
		spinner spn_viewportReduce "Reduce:" range:[0,999,delegate.ViewportReduce] type:#integer fieldwidth:43 offset:[37,-2]
		button btn_viewportReduce_Preset ">" width:18 height:16 align:#right offset:[10,-2] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Request the reduction of the display samples by showing every Nth.\n\nNote that the system might reduce automatically beyond the requested value to maintain usability."
		
		dotNetControl btn_updateChannels "Button" text:"Update Channels" width:114 height:20 align:#right offset:[11,0] tooltip:"Updates the content of the Mask, Color and Display drop-down lists."
		
		label lbl_maskSource "Mask:" across:2 align:#left offset:[2,1] tooltip:"Set the Mask Source"
		dropdownlist ddl_maskSource items:#("No Mask","Density") align:#right width:114 offset:[11,-3]
		
		label lbl_colorSource "Color:" across:2 align:#left offset:[2,1] tooltip:"Set the Color Source"
		dropdownlist ddl_colorSource items:#("Object Color","Color","Velocity") align:#right width:114 offset:[11,-3]

		label lbl_displayVectorSource "Display:" across:2 align:#left offset:[-7,1]
		dropdownlist ddl_displayVectorSource items:#("Large Dots","Velocity") align:#right width:114 offset:[11,-3]
		
		
		checkbox chk_ViewVectorNormalize "Norm.Length" offset:[-10,-2] across:3 tooltip:"Normalize the Vector channel before displaying it." checked:delegate.ViewportVectorNormalize
		spinner spn_viewVectorScale "" range:[0,1000,delegate.viewportVectorScale] fieldwidth:42 offset:[38,-2] tooltip:"Scale the Vector channel before displaying it." 
		button btn_viewVectorScale_Preset ">" width:18 height:16 align:#right offset:[11,-2] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Scale the Vector channel before displaying it." 


		checkbox chk_ViewportBoundsEnabled "Display Grid Bounds" offset:[-10,2]
		
		spinner spn_iconSize "Icon Size " range:[0.0,10000.0,delegate.iconSize] fieldwidth:43 across:2 offset:[60,0] tooltip:"Set the size of the STOKE Viewport Icon." type:#worldunits
		button btn_iconSize_Preset ">" width:18 height:16 align:#right offset:[10,0] images:#(Stoke_PresetsArrowBitmap,Stoke_PresetsArrowBitmap, 32,1,1,2,2) tooltip:"Set the size of the STOKE Viewport Icon."


		fn updateUI =
		(
			chk_viewportenabled.state = delegate.ViewportEnabled
			spn_viewportReduce.value = delegate.ViewportReduce
			spn_iconSize.value = delegate.iconSize
			chk_ViewportBoundsEnabled.state = delegate.ViewportBoundsEnabled
			
			spn_viewVectorScale.value = delegate.viewportVectorScale
			local theScalars = #()
			local theVectors = #()
			try
			(
				local theInterface = stokeGlobalInterface.CreateMXSField selection[1] StackIndex:0
				local theChannels = theInterface.Channels
				for aChannel in theChannels do
				(
					theChannel = (filterString aChannel " []")
					if theChannel[3] == "1" do append theScalars theChannel[1]
					if theChannel[3] == "3" do append theVectors theChannel[1]
				)				
				theInterface.Clear()
			)catch()
			
			local maskSources = #("No Mask")
			for i in theScalars do appendIfUnique maskSources i
			ddl_maskSource.items = maskSources
			
			local colorSources = #("Object Color")
			for i in theVectors do appendIfUnique colorSources i
			for i in theScalars do appendIfUnique colorSources i
			ddl_colorSource.items = colorSources
			
			local displayVectors = #("Large Dots")
			for i in theVectors do appendIfUnique displayVectors i
			ddl_displayVectorSource.items = displayVectors
			
			local theIndex = findItem ddl_MaskSource.items delegate.ViewportMaskChannel
			if theIndex == 0 do theIndex = 1
			ddl_MaskSource.selection = theIndex
			
			local theIndex = findItem ddl_ColorSource.items delegate.ViewportColorChannel
			if theIndex == 0 do theIndex = 1
			ddl_ColorSource.selection = theIndex
			
			local theIndex = findItem ddl_displayVectorSource.items delegate.ViewportVectorChannel
			if theIndex == 0 do theIndex = 1
			ddl_displayVectorSource.selection = theIndex
		)		
		
		fn popupPresetMenu type =
		(
			if createPresetsRCMenu type:type do
				popUpMenu StokeField_Presets_RCMenu position:mouse.screenPos		
		)
		on btn_viewportReduce_Preset pressed do popupPresetMenu #viewportreduce
		on btn_viewportReduce_Preset rightclick do popupPresetMenu #viewportreduce

		on btn_iconSize_Preset pressed do popupPresetMenu #iconSize
		on btn_iconSize_Preset rightclick do popupPresetMenu #iconSize

		on btn_viewVectorScale_Preset pressed do  popupPresetMenu #viewVectorScale
		on btn_viewVectorScale_Preset rightclick do  popupPresetMenu #viewVectorScale
			
		on btn_updateChannels click arg do updateUI()
		
		on chk_ViewportBoundsEnabled  changed state do 
		(
			delegate.ViewportBoundsEnabled = state
		)		
		
		on chk_viewportenabled changed state do 
		(
			delegate.ViewportEnabled = state
			redrawViews()
		)
		
		on ddl_maskSource selected itm do 
		(
			delegate.ViewportMaskChannel = if itm == 1 then "" else ddl_maskSource.selected
			redrawViews()
		)
		on ddl_colorSource selected itm do 
		(
			delegate.ViewportColorChannel = if itm == 1 then "" else ddl_colorSource.selected
			redrawViews()
		)
		on ddl_displayVectorSource selected itm do 
		(
			delegate.ViewportVectorChannel = if itm == 1 then "" else ddl_displayVectorSource.selected
			redrawViews()
		)
		
		on spn_viewVectorScale changed val do 
		(
			delegate.viewportVectorScale = val
			redrawviews()
		)
		on chk_ViewVectorNormalize changed state do 
		(
			delegate.ViewportVectorNormalize = state		
			redrawviews()
		)
		
		
		on spn_iconSize changed val do
		(
			delegate.iconSize = val
			redrawViews()			
		)
		on spn_viewportReduce changed val do 
		(
			delegate.ViewportReduce = val
			redrawViews()	
		)
		
		fn updateLayout =
		(
			btn_updateChannels.FlatStyle = btn_updateChannels.FlatStyle.System
			
			btn_updateChannels.width = ddl_maskSource.width = ddl_colorSource.width = ddl_displayVectorSource.width = display_rollout.width-53
			btn_updateChannels.pos.x = ddl_maskSource.pos.x= ddl_colorSource.pos.x = ddl_displayVectorSource.pos.x = 50
		
			spn_viewportReduce.pos.x = spn_viewVectorScale.pos.x = spn_iconSize.pos.x = display_rollout.width-33
			chk_ViewVectorNormalize.text = if display_rollout.width > 200 then "Normalize Length" else "Norm.Length"
			chk_ViewVectorNormalize.pos.x = 2
		)		
		
		on display_rollout open do 
		(
			if (maxVersion())[1] > 20000 do display_rollout.autoLayoutOnResize = ::Stoke_autoLayoutOnResizeOn	--set to the value of the global variable determined in the Stoke Statup script	if the version is higher than 2018			
			updateLayout()
			updateUI()
		)
	)
	
	tool create 
	( 
		on mousePoint click do
		case click of
		(
			1: (
				nodeTM.translation = gridPoint
				delegate.InWorldSpace = false
				params_rollout.chk_InWorldSpace.state = true
				--params_rollout.btn_getFilename.pressed()
				#stop
			)
		)
	) 
	
	on create do
	(
		delegate.ViewportBoundsEnabled = true
		delegate.InWorldSpace = true
		PlaybackTime.controller = delegate.PlaybackTime.controller = bezier_float() 
	)

	on Load do 
	( 
		
	)	
	
	on clone oldObj do 
	( 
		
	)
	
)

global StokeContextMenuStruct
struct StokeContextMenuStruct (
	fn OpenFieldTexmapMenu owner = 
	(
		global StokeContextMenuOwner = owner
		rcmenu theRCMenu
		(
			menuitem mnu_item10 "SELECT Field Source" enabled:(isValidNode StokeContextMenuOwner.FieldNode)
			separator sep_10
			menuitem mnu_item20 "HIDE Field Source" checked:(isValidNode StokeContextMenuOwner.FieldNode AND StokeContextMenuOwner.FieldNode.isHidden) enabled:(isValidNode StokeContextMenuOwner.FieldNode)
			separator sep_20
			menuitem mnu_item30 "REMOVE Field Source" enabled:(isValidNode StokeContextMenuOwner.FieldNode)
			
			on mnu_item10 picked do if isValidNode ::StokeContextMenuOwner.FieldNode do select ::StokeContextMenuOwner.FieldNode
			on mnu_item20 picked do if isValidNode ::StokeContextMenuOwner.FieldNode do ::StokeContextMenuOwner.FieldNode.isHidden = not ::StokeContextMenuOwner.FieldNode.isHidden 
			on mnu_item30 picked do ::StokeContextMenuOwner.FieldNode = undefined
		)
		popupmenu theRCMenu pos:mouse.screenpos
	),
	fn OpenFieldForceMenu owner = 
	(
		global StokeContextMenuOwner = owner
		rcmenu theRCMenu
		(
			menuitem mnu_item10 "SELECT Field Source" enabled:(isValidNode StokeContextMenuOwner.FieldNode)
			separator sep_10
			menuitem mnu_item20 "HIDE Field Source" checked:(isValidNode StokeContextMenuOwner.FieldNode AND StokeContextMenuOwner.FieldNode.isHidden) enabled:(isValidNode StokeContextMenuOwner.FieldNode)
			separator sep_20
			menuitem mnu_item30 "REMOVE Field Source" enabled:(isValidNode StokeContextMenuOwner.FieldNode)
			
			on mnu_item10 picked do if isValidNode ::StokeContextMenuOwner.FieldNode do select ::StokeContextMenuOwner.FieldNode
			on mnu_item20 picked do if isValidNode ::StokeContextMenuOwner.FieldNode do ::StokeContextMenuOwner.FieldNode.isHidden = not ::StokeContextMenuOwner.FieldNode.isHidden 
			on mnu_item30 picked do ::StokeContextMenuOwner.FieldNode = undefined
		)
		popupmenu theRCMenu pos:mouse.screenpos
	),
	fn OpenFieldMesherMenu owner =
	(
		global StokeContextMenuOwner = owner
		rcmenu theRCMenu
		(
			menuitem mnu_item10 "SELECT Field Source" enabled:(isValidNode StokeContextMenuOwner.TargetNode)
			separator sep_10
			menuitem mnu_item20 "HIDE Field Source" checked:(isValidNode StokeContextMenuOwner.TargetNode AND StokeContextMenuOwner.TargetNode.isHidden) enabled:(isValidNode StokeContextMenuOwner.TargetNode)
			separator sep_20
			menuitem mnu_item30 "ADD RELAX Modifier To The Stack"
			separator sep_30
			menuitem mnu_item40 "CLEAR Material"
			separator sep_40
			
			submenu "ALIGN Field Mesher..."
			(
				menuitem mnu_item50 "ALIGN Field Mesher To Field Source" enabled:(isValidNode StokeContextMenuOwner.TargetNode)
				menuitem mnu_item60 "RESET Field Mesher To World Origin"
			)
			separator sep_70
			menuitem mnu_item70 "REMOVE Field Source" enabled:(isValidNode StokeContextMenuOwner.TargetNode)


			on mnu_item10 picked do if isValidNode ::StokeContextMenuOwner.TargetNode do select ::StokeContextMenuOwner.TargetNode
			on mnu_item20 picked do if isValidNode ::StokeContextMenuOwner.TargetNode do ::StokeContextMenuOwner.TargetNode.isHidden = not ::StokeContextMenuOwner.TargetNode.isHidden 
			on mnu_item30 picked do
			(
				modPanel.addModToSelection (Relax iterations:10)
			)		
			on mnu_item40 picked do
			(
				selection.material = undefined
			)		
		
			on mnu_item50 picked do
			(
				if isValidNode ::StokeContextMenuOwner.TargetNode do
				(
					for o in selection where classof o.baseobject == FieldMesher do
						o.transform = ::StokeContextMenuOwner.TargetNode.transform
				)
			)
			on mnu_item60 picked do
			(
				for o in selection where classof o.baseobject == FieldMesher do
					o.transform = matrix3 1
			)

			on mnu_item70 picked do 
			(
				::StokeContextMenuOwner.TargetNode = undefined
				modPanel.setCurrentObject ::StokeContextMenuOwner
			)
		)
		popupmenu theRCMenu pos:mouse.screenpos
	),
	fn OpenPRTFieldMenu owner = 
	( 
		global StokeContextMenuOwner = owner
		rcmenu theRCMenu
		(
			menuItem mnu_pdv "OPEN Particle Data VIEWER..."
			separator sep_05
			menuitem mnu_item10 "SELECT Field Source" enabled:(isValidNode StokeContextMenuOwner.TargetNode)
			separator sep_10
			menuitem mnu_item20 "HIDE Field Source" checked:(isValidNode StokeContextMenuOwner.TargetNode AND StokeContextMenuOwner.TargetNode.isHidden) enabled:(isValidNode StokeContextMenuOwner.TargetNode)
			separator sep_20
			menuitem mnu_item30 "REMOVE Field Source" enabled:(isValidNode StokeContextMenuOwner.TargetNode)

			on mnu_pdv picked do 
			(
				try(fileIn (FranticParticles.KrakatoaHome+"Scripts\\Krakatoa_ParticleDataViewer.ms"))catch()
			)
			on mnu_item10 picked do if isValidNode ::StokeContextMenuOwner.TargetNode do select ::StokeContextMenuOwner.TargetNode
			on mnu_item20 picked do if isValidNode ::StokeContextMenuOwner.TargetNode do ::StokeContextMenuOwner.TargetNode.isHidden = not ::StokeContextMenuOwner.TargetNode.isHidden 
			on mnu_item30 picked do 
			(
				::StokeContextMenuOwner.TargetNode = undefined
				modPanel.setCurrentObject ::StokeContextMenuOwner
			)
		)
		popupmenu theRCMenu pos:mouse.screenpos
	),
	fn OpenFieldFollowMenu owner = 
	( 
		global StokeContextMenuOwner = owner
		rcmenu theRCMenu
		(
			menuitem mnu_item10 "SELECT Field Source" enabled:(isValidNode StokeContextMenuOwner.FieldNode)
			separator sep_10
			menuitem mnu_item20 "HIDE Field Source" checked:(isValidNode StokeContextMenuOwner.FieldNode AND StokeContextMenuOwner.FieldNode.isHidden) enabled:(isValidNode StokeContextMenuOwner.FieldNode)
			separator sep_20
			menuitem mnu_item30 "REMOVE Field Source" enabled:(isValidNode StokeContextMenuOwner.FieldNode)
			
			on mnu_item10 picked do if isValidNode ::StokeContextMenuOwner.FieldNode do select ::StokeContextMenuOwner.FieldNode
			on mnu_item20 picked do if isValidNode ::StokeContextMenuOwner.FieldNode do ::StokeContextMenuOwner.FieldNode.isHidden = not ::StokeContextMenuOwner.FieldNode.isHidden 
			on mnu_item30 picked do 
			(
				::StokeContextMenuOwner.FieldNode = undefined
				modPanel.setCurrentObject ::StokeContextMenuOwner
			)
		)
		popupmenu theRCMenu pos:mouse.screenpos
	)
)