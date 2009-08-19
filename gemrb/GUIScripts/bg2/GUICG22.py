# -*-python-*-
# GemRB - Infinity Engine Emulator
# Copyright (C) 2003-2007 The GemRB Project
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# $Id$
#character generation, class kit (GUICG22)

import GemRB
from GUICommonWindows import ClassTable, KitListTable, RaceTable

KitWindow = 0
TextAreaControl = 0
DoneButton = 0
SchoolList = 0
ClassID = 0
TopIndex = 0
RowCount = 10
KitTable = 0
Init = 0

def OnLoad():
	global KitWindow, TextAreaControl, DoneButton
	global SchoolList, ClassID
	global RowCount, TopIndex, KitTable, Init
	
	GemRB.LoadWindowPack("GUICG", 640, 480)
	RaceName = RaceTable.GetRowName(GemRB.GetVar("Race")-1 )
	Class = GemRB.GetVar("Class")-1
	ClassName = ClassTable.GetRowName(Class)
	ClassID = ClassTable.GetValue(Class, 5)
	KitTable = GemRB.LoadTableObject("kittable")
	KitTableName = KitTable.GetValue(ClassName, RaceName)
	KitTable = GemRB.LoadTableObject(KitTableName,1)

	SchoolList = GemRB.LoadTableObject("magesch")

	#there is a specialist mage window, but it is easier to use
	#the class kit window for both
	KitWindow = GemRB.LoadWindowObject(22)
	if ClassID == 1:
		Label = KitWindow.GetControl(0xfffffff)
		Label.SetText(595)

	for i in range(10):
		if i<4:
			Button = KitWindow.GetControl(i+1)
		else:
			Button = KitWindow.GetControl(i+5)
		Button.SetState(IE_GUI_BUTTON_DISABLED)
		Button.SetFlags(IE_GUI_BUTTON_RADIOBUTTON, OP_OR)

	if not KitTable:
		RowCount = 1
	else:
		if ClassID == 1:
			RowCount = SchoolList.GetRowCount()
		else:
			RowCount = KitTable.GetRowCount()

	TopIndex = 0
	GemRB.SetVar("TopIndex", 0)
	if RowCount>10:
		KitWindow.CreateScrollBar(1000, 290, 47, 16, 200)
		ScrollBar = KitWindow.GetControl (1000)
		ScrollBar.SetSprites("GUISCRCW", 0, 0,1,2,3,5,4)
		ScrollBar.SetVarAssoc("TopIndex",RowCount-10)
		ScrollBar.SetEvent(IE_GUI_SCROLLBAR_ON_CHANGE, "RedrawKits")
		ScrollBar.SetDefaultScrollBar()
		RowCount=10

	for i in range(RowCount):
		if i<4:
			Button = KitWindow.GetControl(i+1)
		else:
			Button = KitWindow.GetControl(i+5)

		if not KitTable:
			if ClassID == 1:
				# TODO: this seems to be never reached
				Kit = GemRB.GetVar("MAGESCHOOL")
				KitName = SchoolList.GetValue(i+TopIndex, 0)
				Kit = SchoolList.GetValue (Kit, 3)
			else:
				Kit = 0
				KitName = ClassTable.GetValue(GemRB.GetVar("Class")-1, 0)

		else:
			Kit = KitTable.GetValue (i+TopIndex, 0)
			if ClassID == 1:
				KitName = SchoolList.GetValue (i+TopIndex, 0)
				if Kit == 0:
					KitName = SchoolList.GetValue (0, 0)
				elif Kit == "*":
					Kit = 0
			else:
				if Kit:
					KitName = KitListTable.GetValue(Kit, 1)
				else:
					KitName = ClassTable.GetValue(GemRB.GetVar("Class")-1, 0)
		Button.SetState(IE_GUI_BUTTON_ENABLED)
		Button.SetText(KitName)
		Button.SetVarAssoc("ButtonPressed", i)
		Button.SetEvent(IE_GUI_BUTTON_ON_PRESS, "KitPress")

	BackButton = KitWindow.GetControl(8)
	BackButton.SetText(15416)
	BackButton.SetFlags (IE_GUI_BUTTON_CANCEL, OP_OR)
	DoneButton = KitWindow.GetControl(7)
	DoneButton.SetText(11973)
	DoneButton.SetFlags(IE_GUI_BUTTON_DEFAULT,OP_OR)

	TextAreaControl = KitWindow.GetControl(5)
	TextAreaControl.SetText(17247)

	DoneButton.SetEvent(IE_GUI_BUTTON_ON_PRESS,"NextPress")
	BackButton.SetEvent(IE_GUI_BUTTON_ON_PRESS,"BackPress")
	Init = 1
	RedrawKits()
	Init = 0
	KitPress()
	KitWindow.SetVisible(1)
	return

def RedrawKits():
	global TopIndex

	TopIndex=GemRB.GetVar("TopIndex")
	EnabledButtons = []
	for i in range(RowCount):
		if i<4:
			Button = KitWindow.GetControl(i+1)
		else:
			Button = KitWindow.GetControl(i+5)

		Button.SetState(IE_GUI_BUTTON_DISABLED)
		if not KitTable:
			if ClassID == 1:
				Kit = GemRB.GetVar("MAGESCHOOL")
				KitName = SchoolList.GetValue(i+TopIndex, 0)
				Kit = SchoolList.GetValue (Kit, 3)
			else:
				Kit = 0
				KitName = ClassTable.GetValue(GemRB.GetVar("Class")-1, 0)

		else:
			Kit = KitTable.GetValue (i+TopIndex,0)
			if ClassID == 1:
				KitName = SchoolList.GetValue (i+TopIndex, 0)
				if Kit != "*":
					EnabledButtons.append(Kit-21)
				if Kit == 0:
					KitName = SchoolList.GetValue (0, 0)
					Button.SetState(IE_GUI_BUTTON_ENABLED)
			else:
				if Kit:
					KitName = KitListTable.GetValue(Kit, 1)
				else:
					KitName = ClassTable.GetValue(GemRB.GetVar("Class")-1, 0)

		Button.SetText(KitName)
		if not EnabledButtons or i+TopIndex in EnabledButtons:
			Button.SetState(IE_GUI_BUTTON_ENABLED)
		if Kit == "*":
			continue
		if i+TopIndex==0 and Init:
			GemRB.SetVar("ButtonPressed", i)
	return

def KitPress():
	global ClassID

	ButtonPressed=GemRB.GetVar("ButtonPressed")

	if not KitTable:
		if ClassID == 1: 
			# TODO: this seems to be never reached
			Kit = GemRB.GetVar("MAGESCHOOL")
			KitName = SchoolList.GetValue(ButtonPressed+TopIndex, 0)
			Kit = SchoolList.GetValue (Kit, 3)
		else:
			Kit = 0
			KitName = ClassTable.GetValue(GemRB.GetVar("Class")-1, 0)
	else:
		Kit = KitTable.GetValue (ButtonPressed+TopIndex, 0)
		if ClassID == 1:
			KitName = SchoolList.GetValue (ButtonPressed+TopIndex, 0)
			if Kit == 0:
				KitName = SchoolList.GetValue (0, 0)
			elif Kit == "*":
				Kit = 0
		else:
			if Kit:
				KitName = KitListTable.GetValue(Kit, 1)
			else:
				KitName = ClassTable.GetValue(GemRB.GetVar("Class")-1, 0)

	if ClassID == 1 and Kit != 0:
		GemRB.SetVar("MAGESCHOOL", Kit-21) # hack: -21 to make the generalist 0
	else:
		GemRB.SetVar("MAGESCHOOL", 0) # so bards don't get schools

	if Kit == 0:
		KitName = ClassTable.GetValue(GemRB.GetVar("Class")-1, 1)
	else:
		KitName = KitListTable.GetValue(Kit, 3)

	TextAreaControl.SetText(KitName)
	DoneButton.SetState(IE_GUI_BUTTON_ENABLED)

	GemRB.SetVar("Class Kit", Kit)

	return

def BackPress():
	GemRB.SetVar("Class Kit",0) #scrapping
	GemRB.SetVar("MAGESCHOOL", 0)
	if KitWindow:
		KitWindow.Unload()
	GemRB.SetNextScript("GUICG2")
	return

def NextPress():
	if KitWindow:
		KitWindow.Unload()
	GemRB.SetNextScript("CharGen4") #abilities
	return
