-- Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
-- SPDX-License-Identifier: Apache-2.0
macroScript StokeFieldProbe category:"Stoke" buttontext:"Probe" icon:#("Stoke",9) tooltip:"Open the Stoke Field Probe dialog."
(
global StokeField_Probe_Dialog
try(destroyDialog StokeField_Probe_Dialog)catch()
local theIniFile = (GetDir #plugcfg + "\\StokeFieldProble_Preferences.ini" )	
local limitVertexCount = #all
local showMaxVerticesCount = 1000
local useMeshVertices = false
local Stoke_FieldProbe_ViewportDisplayData = #{}
local viewportDisplayDataEnabler=#{1}	
local theChannelNames = #()	
--local useSplineKnots = false
	
local theVal = getIniSetting theIniFile "SampleOptions" "LimitVertexCount"
if theVal != "" do limitVertexCount = theVal as name
local theVal = getIniSetting theIniFile "SampleOptions" "MaxVerticesCount"
if theVal != "" do showMaxVerticesCount = execute theVal 

rcmenu FieldProbe_MainMenu 
(
	subMenu "Field Sources"
	(
		menuItem mnu_updateSourcesList "Update Field Sources List"
	)
	on mnu_updateSourcesList picked do StokeField_Probe_Dialog.updateSourcesList()
	
	subMenu "Probe Options"
	(
		menuItem mnu_useMeshVertices "Use MESH Vertices As Probe Points" checked:useMeshVertices
		--menuItem mnu_useSplineKnots "Use SPLINE Vertices As Probe Points" checked:useSplineKnots enabled:false
		separator sep_10
		menuItem mnu_showAllVertices "Use ALL Vertices" checked:(limitVertexCount == #all)
		menuItem mnu_showFirstNVerts "Use FIRST N Vertices" checked:(limitVertexCount == #limit)
		menuItem mnu_showNtVert "Use EVERY Nth Vertex" checked:(limitVertexCount == #nth)
		separator sep_20
		menuItem mnu_show10 "Limit to 10 Vertices" checked:(showMaxVerticesCount == 10)
		menuItem mnu_show100 "Limit to 100 Vertices" checked:(showMaxVerticesCount == 100)
		menuItem mnu_show1000 "Limit to 1000 Vertices" checked:(showMaxVerticesCount == 1000)
	)
	
	subMenu "Viewport Display"
	(
		menuItem mnu_displayProbeNames "Display NAMES Of Highlighted Probes" checked:(viewportDisplayDataEnabler[1] == True)
		menuItem mnu_displayProbePositions "Display POSITIONS Of Highlighted Probes" checked:(viewportDisplayDataEnabler[2] == True)
		separator sep_30
		menuItem mnu_displayAllChannels "Display ALL Channels Of Highlighted Probes" checked:((theChannelNames.count+2) == viewportDisplayDataEnabler.numberSet)
		menuItem mnu_displayNoChannels "Display NO Channels Of Highlighted Probes" checked:(viewportDisplayDataEnabler.numberSet < 3)
		separator sep_40
		menuItem mnu_infopopup "[?] Learn More About Toggling Channels Display On And Off..." 
	)
	
	subMenu "Highlight Probes"
	(
		menuItem mnu_highlightall "Highlight ALL Probes"
		menuItem mnu_highlightInvert "INVERT Current Highlights "
	)
	
	on mnu_highlightall picked do 
	(
		theSel = #{1..StokeField_Probe_Dialog.dnc_datalistview.items.count}
		StokeField_Probe_Dialog.setListViewSelection (theSel as array)
	)	
	
	on mnu_highlightInvert picked do 
	(
		theSel = (StokeField_Probe_Dialog.getListViewSelection()) as bitarray
		theSel = #{1..StokeField_Probe_Dialog.dnc_datalistview.items.count}-theSel
		StokeField_Probe_Dialog.setListViewSelection (theSel as array)
	)
	
	on mnu_displayProbeNames picked do viewportDisplayDataEnabler[1] = not viewportDisplayDataEnabler[1]
	on mnu_displayProbePositions picked do viewportDisplayDataEnabler[2] = not viewportDisplayDataEnabler[2]
	
	on mnu_displayAllChannels picked do viewportDisplayDataEnabler = #{1..(theChannelNames.count+2)}
	on mnu_displayNoChannels picked do viewportDisplayDataEnabler = #{1}
		
	on mnu_infopopup picked do 
	(
		messagebox "SHIFT+Click a Column Header to TOGGLE the display.\nSHIFT+ALT+Click a Column Header to show ALL.\nSHIFT+CTRL+Click a Column Header to show NONE." title:"Probe Info - Displaying Data In The Viewport"
	)
		
	
	on mnu_useMeshVertices picked do 
	(
		useMeshVertices = not useMeshVertices
		Stoke_FieldProbe_ViewportDisplayData = #{}
		StokeField_Probe_Dialog.updateOnTimeChange()
	)
	/*on mnu_useSplineKnots picked do 
	(
		useSplineKnots = not useSplineKnots
		StokeField_Probe_Dialog.updateOnTimeChange()
	)*/
	
	on mnu_showAllVertices picked do
	(
		limitVertexCount = #all
		setIniSetting theIniFile "SampleOptions" "LimitVertexCount" (limitVertexCount as string)
		Stoke_FieldProbe_ViewportDisplayData = #{}
		StokeField_Probe_Dialog.updateOnTimeChange()
	)
	on mnu_showFirstNVerts picked do
	(
		limitVertexCount = #limit
		setIniSetting theIniFile "SampleOptions" "LimitVertexCount" (limitVertexCount as string)
		Stoke_FieldProbe_ViewportDisplayData = #{}
		StokeField_Probe_Dialog.updateOnTimeChange()
	)
	on mnu_showNtVert picked do
	(
		limitVertexCount = #nth
		setIniSetting theIniFile "SampleOptions" "LimitVertexCount" (limitVertexCount as string)
		Stoke_FieldProbe_ViewportDisplayData = #{}
		StokeField_Probe_Dialog.updateOnTimeChange()
	)
		
	on mnu_show10 picked do 
	(
		showMaxVerticesCount = 10
		setIniSetting theIniFile "SampleOptions" "MaxVerticesCount" (showMaxVerticesCount as string)
		Stoke_FieldProbe_ViewportDisplayData = #{}
		StokeField_Probe_Dialog.updateOnTimeChange()
	)
	on mnu_show100 picked do 
	(
		showMaxVerticesCount = 100
		setIniSetting theIniFile "SampleOptions" "MaxVerticesCount" (showMaxVerticesCount as string)
		Stoke_FieldProbe_ViewportDisplayData = #{}
		StokeField_Probe_Dialog.updateOnTimeChange()
	)	
	on mnu_show1000 picked do 
	(
		showMaxVerticesCount = 1000
		setIniSetting theIniFile "SampleOptions" "MaxVerticesCount" (showMaxVerticesCount as string)
		Stoke_FieldProbe_ViewportDisplayData = #{}
		StokeField_Probe_Dialog.updateOnTimeChange()
	)		
	
)	

rollout StokeField_Probe_Dialog "Stoke Field Probe"
(
	local theProbes = #()
	local theProbesOnList = #()
	local magmaFieldsInScene = #()
	local currentStream = undefined
	local theChannels = #()
	
	dropdownlist ddl_fieldsToProbe items:#() pos:[0,0] width:220 
	button btn_addSelected "ADD Selected As Probes"  pos:[220,0] width:130
	button btn_removeSelected "REMOVE Highlighted Probes"  pos:[350,0] width:145
	checkbutton chk_autoUpdate ">AUTO" align:#right offset:[14,-4] pos:[495,0] width:45
	button btn_update "UPDATE" align:#right pos:[540,0] width:60
	
	dotNetControl dnc_datalistview "ListView" pos:[0,22] width:600 height:400 
	
	fn updateSourcesList =
	(
		magmaFieldsInScene = for o in objects where StokeGlobalInterface.GetVelocityType o == #field  collect o --findItem #(Stoke_Field,StokeFieldLoader) (classof o.baseobject) > 0
		local oldSel = ddl_fieldsToProbe.selected
		ddl_fieldsToProbe.items = for o in magmaFieldsInScene collect o.name
		if oldSel != undefined do ddl_fieldsToProbe.selection = findItem ddl_fieldsToProbe.items oldSel
	)	
	
	fn getColumnSize channelName defaultValue:100=
	(
		local theResult = case channelName of
		(
			default: defaultValue
			"ID": 70
		)
		local theVal = execute (getIniSetting theIniFile "FieldProbeColumnWidth" channelName )
		if theVal != OK do theResult = theVal
		theResult
	)	
	
	on dnc_datalistview ColumnWidthChanged arg do 
	(
		local theIndex = arg.ColumnIndex
		local theText = dnc_datalistview.Columns.Item[theIndex].Text
		theText = (filterString theText " ")[1]
		local theWidth = dnc_datalistview.Columns.Item[theIndex].Width
		setIniSetting theIniFile "FieldProbeColumnWidth" theText (theWidth as string)
	)	
	
	fn getListViewSelection =	--returns an array of the selected ListView items
	(
		try
			for i = 1 to dnc_datalistview.items.count where dnc_datalistview.items.item[i-1].Selected collect i
		catch 
			#()
	)	
	
	fn setListViewSelection theSel =
	(
		for i = 1 to dnc_datalistview.items.count do dnc_datalistview.items.item[i-1].Selected = findItem theSel i > 0 
		dnc_datalistview.focus()
	)		
	
	fn initListView =
	(
		local lv = dnc_datalistview
		local layout_def = #(#("Probe",getColumnSize "Probe"),#("Position", getColumnSize "Position" defaultValue:150))
			
		for aChannel in theChannels do
		(				
			local theColumnWidth = 60
			if aChannel[3] == 3 do theColumnWidth = 150
			theColumnWidth = getColumnSize aChannel[1] defaultValue:theColumnWidth
			append layout_def #(aChannel[1], theColumnWidth)
		)
		lv.Clear()
		lv.backColor = (dotNetClass "System.Drawing.Color").fromARGB 221 221 225
		lv.View = (dotNetClass "System.Windows.Forms.View").Details
		lv.gridLines = true 
		lv.fullRowSelect = true 
		lv.MultiSelect = true
		lv.checkboxes = false
		lv.hideSelection = false		
		for i in layout_def do
			lv.Columns.add i[1]  i[2]
	)
	
	fn populateListView =
	(
		local theSel = getListViewSelection()
		theProbesOnList = #()
		local lv = dnc_datalistview
		lv.Update()
		lv.Items.Clear()
		lv.SuspendLayout()
		theRange = #()
		for aProbe in theProbes where isValidNode aProbe do
		(
			local done = false
			if useMeshVertices == true AND findItem GeometryClass.classes (classof aProbe) > 0 AND classof aProbe != TargetObject do
			(
				theMesh = snapshotAsMesh aProbe
				if theMesh != undefined and theMesh.numverts > 0 do
				(
					local theStep = 1
					local theCount = theMesh.numverts
					if theCount > showMaxVerticesCount do 
						case limitVertexCount of  
						(
							#limit: (
								theCount = showMaxVerticesCount
							)
							#nth: (
								theStep = (ceil (1.0 * theCount / showMaxVerticesCount)) as integer
							)
							default: ( ) --do nothing, use them all
						)
					for v = 1 to theCount by theStep do
					(
						li = dotNetObject "System.Windows.Forms.ListViewItem" (aProbe.name + ":vert "+v as string)
						append theProbesOnList aProbe
						local thePos = (getVert theMesh v)
						li.SubItems.add (thePos as string)
						for aChannel in theChannelNames do
						(
							local val = currentStream.SampleField aChannel thePos
							if val == undefined do val = "--no data--"
							li.SubItems.add (val as string)
						)	
						append theRange li							
					)
					delete theMesh
					done = true
				)
			)
			if not done do
			(
				append theProbesOnList aProbe
				li = dotNetObject "System.Windows.Forms.ListViewItem" (aProbe.name)
				li.SubItems.add (aProbe.pos as string)
				for aChannel in theChannelNames do
				(
					local val = currentStream.SampleField aChannel aProbe.pos
					if val == undefined do val = "--no data--"
					li.SubItems.add (val as string)
				)
				append theRange li							
			)
		)
		lv.Items.AddRange theRange 
		lv.ResumeLayout false
		setListViewSelection theSel 
		if theSel.count > 0 do
			dnc_datalistview.EnsureVisible (theSel[1]-1)		
	)
	

	fn createStream =
	(
		currentStream = at time currentTime StokeGlobalInterface.CreateMXSField magmaFieldsInScene[ddl_fieldsToProbe.selection]
		theChannels = currentStream.Channels
		for i = 1 to theChannels.count do ( 
			theChannels[i] = filterString theChannels[i] " []"
			if theChannels[i].count == 3 then (
				theChannels[i][3] = theChannels[i][3] as integer
			) else (
				--This happens with "Position float32" without an arity. We assume it is 1
				theChannels[i][3] = 1
			)
		)
		theChannelNames = for i in theChannels collect i[1]
	)
	
	fn updateCurrentDisplaySelection =
	(
		Stoke_FieldProbe_ViewportDisplayData = #{}
		for i = dnc_datalistview.items.count-1 to 0 by -1 do
			Stoke_FieldProbe_ViewportDisplayData[i+1] = dnc_datalistview.items.item[i].selected 
	)
	
	on dnc_datalistview ItemSelectionChanged args do
	( 
		updateCurrentDisplaySelection()
		--max views redraw
		/*
		if not dnc_datalistview.items.item[args.item.index].selected then 
			Stoke_FieldProbe_ViewportDisplayData[args.item.index+1] = false
		else
			Stoke_FieldProbe_ViewportDisplayData[args.item.index+1] = true
		*/
	)	
	
	on dnc_datalistview ColumnClick args do
	(
		if keyboard.shiftPressed then
		(
			if keyboard.controlPressed then
				viewportDisplayDataEnabler=#{1}
			else
			if keyboard.altPressed then
				viewportDisplayDataEnabler=#{1..(theChannelNames.count+2)}
			else
				viewportDisplayDataEnabler[args.column+1] = not viewportDisplayDataEnabler[args.column+1]
			max views redraw
		)
		--updateChannelTitles()
	)	
	
	fn redrawViewsFunction =
	(
		gw.setTransform (matrix3 1)
		for i in Stoke_FieldProbe_ViewportDisplayData where i <= dnc_datalistview.items.count do
		(
			local thePos = (execute dnc_datalistview.items.item[i-1].subItems.item[1].text)
			gw.Marker thePos #circle color:red
			theText = ""
			local cnt = 0
			for k in viewportDisplayDataEnabler do
			(
				theName =if k == 1 then "" else if k == 2 then "Position:" else theChannelNames[k-2] + ":"
				theText = theName + dnc_datalistview.items.item[i-1].subItems.item[k-1].text +"  "
				gw.wText (gw.wTransPoint (thePos+[0,0,-1*(cnt)])) theText color:yellow
				
				/*	
				if DisplayVectorsInViewport == true and k > 1 and theChannels[k-1][3] == 3 do
					try (gw.Polyline #(thePos, thePos+((execute dnc_datalistview.items.item[i-1].subItems.item[k-1].text)*ScaleVectorsInViewport) ) false )catch()
				*/	
				cnt+=1
			)
		)
		gw.enlargeUpdateRect #whole 
		gw.updateScreen()
	)	
	
	fn updateOnTimeChange =
	(
		if ddl_fieldsToProbe.selection > 0 AND isValidNode magmaFieldsInScene[ddl_fieldsToProbe.selection] do
		(
			local theSel = getListViewSelection()
			try(magmaFieldsInScene[ddl_fieldsToProbe.selection].delegate.MagmaHolder.autoUpdate = magmaFieldsInScene[ddl_fieldsToProbe.selection].delegate.MagmaHolder.autoUpdate)catch()
			try(currentStream.clear())catch()
			createStream()
			initListView()
			populateListView()
			setListViewSelection theSel
			if theSel.count > 0 and theSel[1] < dnc_datalistview.items.count do
				dnc_datalistview.EnsureVisible (theSel[1]-1)
		)
	)
	
	fn unregisterCallbacks =
	(
		unregisterTimeCallback updateOnTimeChange
		deleteAllChangeHandlers id:#StokeFieldProbe
	)
	fn registerCallbacks =
	(
		unregisterCallbacks()
		registerTimeCallback updateOnTimeChange
		when transform theProbes change handleAt:#redrawViews id:#StokeFieldProbe do
		(
			populateListView()
		)
	)
	
	on btn_update pressed do
	(
		updateOnTimeChange()
	)
	
	on btn_addSelected pressed do
	(
		for o in selection do appendIfUnique theProbes o
		populateListView()
	)
	
	on btn_removeSelected pressed do
	(
		for i = dnc_datalistview.items.count-1 to 0 by -1 do
			if dnc_datalistview.items.item[i].selected do 
			(
				local theIndex = findItem theProbes theProbesOnList[i+1]
				if theIndex > 0 do deleteItem theProbes theIndex
			)
		setListViewSelection #()	
		populateListView()
	)
	
	on ddl_fieldsToProbe selected itm do
	(
		unregisterCallbacks()
		try(currentStream.clear())catch()
		createStream()
		initListView()
		populateListView()
		Stoke_FieldProbe_ViewportDisplayData = #{}
		viewportDisplayDataEnabler=#{1}				
		if chk_autoUpdate.state do registerCallbacks()		
	)
	
	on StokeField_Probe_Dialog open do
	(
		updateSourcesList()
		initListView()
		if magmaFieldsInScene.count > 0 do
		(
			createStream()
			initListView()
			theProbes = for o in selection collect o
			populateListView()
			--registerCallbacks()
		)
		registerRedrawViewsCallback redrawViewsFunction
		
		--showProperties dnc_datalistview
		--showMethods dnc_datalistview
	)
	
	on chk_autoUpdate changed state do
	(
		if state then registerCallbacks() else unregisterCallbacks()
	)
	
	on StokeField_Probe_Dialog close do
	(
		try(currentStream.clear())catch()
		unregisterCallbacks()
		unregisterRedrawViewsCallback redrawViewsFunction	
	)
	
	fn resizeDialog val =
	(
		dnc_datalistview.width = val.x
		dnc_datalistview.height = val.y-22
	)		
	
	on StokeField_Probe_Dialog resized val do
	(
		resizeDialog val 
		setIniSetting theIniFile "Dialog" "Size" (val as string)
	)
	
	on StokeField_Probe_Dialog moved pos do
	(
		setIniSetting theIniFile "Dialog" "Position" (pos as string)
	)
)

local theX = 600
local theY = 400
local theVal = execute (getIniSetting theIniFile "Dialog" "Size")
if theVal != OK do 
(
	theX = theVal.x 
	theY = theVal.y
	if theX < 600 do theX = 600
	if theY < 400 do theY = 400
)

theVal = execute (getIniSetting theIniFile "Dialog" "Position")
	if theVal != OK then
	(
		if theVal.x < 0 do theVal.x = 0
		if theVal.y < 0 do theVal.y = 0
		if theVal.x > sysinfo.desktopsize.x-theX do theVal.x  = sysinfo.desktopsize.x-theX
		if theVal.y > sysinfo.desktopsize.y-theY do theVal.y  = sysinfo.desktopsize.y-theY
		createDialog StokeField_Probe_Dialog theX theY theVal.x theVal.y style:#(#style_titlebar, #style_border, #style_sysmenu, #style_minimizebox, #style_resizing, #style_maximizebox ) menu:FieldProbe_MainMenu 
		StokeField_Probe_Dialog.resizeDialog [theX, theY]
	)
	else
		createDialog StokeField_Probe_Dialog theX theY style:#(#style_titlebar, #style_border, #style_sysmenu, #style_minimizebox, #style_resizing, #style_maximizebox ) menu:FieldProbe_MainMenu 
)