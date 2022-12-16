-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
macroScript Stoke2FGAExporter category:"Stoke" buttontext:"STK2FGA" tooltip:"Export a Stoke Vector Field to a Field Grid ASCII (FGA) File"
(
	global Stoke2FGA_Exporter 
	try(destroyDialog Stoke2FGA_Exporter )catch()
	rollout Stoke2FGA_Exporter "Stoke To FGA Exporter"
	(
		local theStokeFieldObjects = #()
		
		dropdownlist ddl_fields "Stoke Field Object To Export:" items:#()
		dropdownlist ddl_channels "Vector Field To Export:" items:#()
		radiobuttons rad_timeMode labels:#("Divide by FPS (Units Per Frame)", "Unmodified")
		checkbox chk_editFile "Open File After Export"
		progressbar prg_bar height:6 color:red width:200 align:#center
		button btn_export "EXPORT STOKE FIELD TO FGA" width:200 height:30 enabled:false align:#center
		
		fn getChannelsList currentStream =
		(
			local theChannels = currentStream.Channels
			for i = 1 to theChannels.count do ( 
				theChannels[i] = filterString theChannels[i] " []"
				if theChannels[i].count == 3 then (
					theChannels[i][3] = theChannels[i][3] as integer
				) else (
					--This happens with "Position float32" without an arity. We assume it is 1
					theChannels[i][3] = 1
				)
			)
			theChannels
		)
		
		fn updateStokeFields =
		(
			theStokeFieldObjects = for o in objects where StokeGlobalInterface.IsEmberField o collect o
			ddl_fields.items = for o in theStokeFieldObjects collect o.name
			if selection.count == 1 and (theIndex = (findItem theStokeFieldObjects selection[1])) > 0 do ddl_fields.selection = theIndex
		)
		
		fn updateUI forceVelocity:true =
		(
			if ddl_fields.selection > 0 do
			(
				local currentStream = StokeGlobalInterface.CreateMXSField theStokeFieldObjects[ddl_fields.selection]
				local theChannels = getChannelsList currentStream
				ddl_channels.items = for c in theChannels where c[3] == 3 collect c[1]
				if forceVelocity then
				(
					local theIndex = findItem ddl_channels.items "Velocity"
					if theIndex == 0 do theIndex = 1
					ddl_channels.selection = theIndex	
				)
				rad_timeMode.state = if ddl_channels.selected == "Velocity" then 1 else 2
				currentStream.Clear()
			)
			btn_export.enabled = theStokeFieldObjects.count > 0 and ddl_channels.items.count > 0
		)
		
		on ddl_fields selected itm do updateUI forceVelocity:true
		on ddl_channels selected itm do updateUI forceVelocity:false
		
		on btn_export pressed do
		(
			local theFileName = getSaveFileName caption:"Export To FGA Field ASCII File" types:"Field Grid ASCII (*.fga)|*.fga|All Files (*.*)|*.*"
			if theFileName == undefined do return false
			
			local st = timestamp()
			local currentStream = StokeGlobalInterface.CreateMXSField theStokeFieldObjects[ddl_fields.selection]
			local theChannelNames= #()	
			local theVoxel = [1,1,1]*currentStream.Spacing
			local theHalfVoxel = theVoxel/2.0
			local bbox = ((currentStream.BoundsMax-theHalfVoxel)-(currentStream.BoundsMin+theHalfVoxel))/currentStream.Spacing
			local theCount = (ceil bbox.x * ceil bbox.y * ceil bbox.z ) as integer
			local done = false
			local theChannels = getChannelsList currentStream
			local channelToExport = ddl_channels.selected
			local allFileChannels = for i in theChannels collect i[1]
			if findItem allFileChannels channelToExport  > 0 then
			(
				local theStringStream = "" as stringStream
				format "%,%,%,\n" ((ceil bbox.x) as integer) ((ceil bbox.y) as integer) ((ceil bbox.z) as integer) to:theStringStream
				format "%,%,%,%,%,%,\n" currentStream.BoundsMin.x currentStream.BoundsMin.y currentStream.BoundsMin.z currentStream.BoundsMax.x currentStream.BoundsMax.y currentStream.BoundsMax.z to:theStringStream
				local cnt = 0
				for z = 0 to bbox.z do
				(
					for y = 0 to bbox.y do
					(
						windows.processPostedMessages()
						for x = 0 to bbox.x do
						(
							cnt += 1
							prg_bar.value = 100.0*cnt/theCount
							local samplePos = [currentStream.BoundsMin.x+theHalfVoxel.x+currentStream.Spacing*x, currentStream.BoundsMin.y+theHalfVoxel.y+currentStream.Spacing*y,currentStream.BoundsMin.z+theHalfVoxel.z+currentStream.Spacing*z]
							local val = currentStream.SampleField channelToExport samplePos
							if val == undefined do val = [0,0,0]
							if rad_timeMode.state == 1 do val /= FrameRate
							format "%,%,%,\n" val.x val.y val.z to:theStringStream
						)--end x loop
					)--end y loop
				)--end z loop
				(dotnetclass "System.IO.File").WriteAllText theFileName (theStringStream as string) 
				format "--FGA Export of Field '%' to [%] with % samples took % seconds.\n" channelToExport theFileName theCount ((timestamp()-st)/1000.0)
			)--end if
			else
				messagebox ("No '"+channelToExport+"' Field Found In The Specified Object!") title:"FGA Export Error"
			if chk_editFile.checked do edit theFileName
			currentStream.Clear()
			prg_bar.value = 0
			gc light:true
		)--end on button pressed
		
		on Stoke2FGA_Exporter open do
		(
			updateStokeFields()
			updateUI()
		)
	)--end rollout
	createDialog Stoke2FGA_Exporter 210 200
)--end macro