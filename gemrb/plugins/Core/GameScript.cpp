/* GemRB - Infinity Engine Emulator
 * Copyright (C) 2003 The GemRB Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * $Header: /data/gemrb/cvs2svn/gemrb/gemrb/gemrb/plugins/Core/GameScript.cpp,v 1.21 2003/12/25 23:55:08 balrog994 Exp $
 *
 */

#include "GameScript.h"
#include "Interface.h"
#include "DialogMgr.h"

extern Interface * core;
int initialized = 0;

static Variables * globals;
static SymbolMgr* triggersTable;
static SymbolMgr* actionsTable;
static TriggerFunction triggers[MAX_TRIGGERS];
static ActionFunction actions[MAX_ACTIONS];
static bool blocking[MAX_ACTIONS];

GameScript::GameScript(const char * ResRef, unsigned char ScriptType, Variables * local)
{
	if(local) {
		locals = local;
		freeLocals = false;
	}
	else {
		locals = new Variables();
		locals->SetType(GEM_VARIABLES_INT);
		freeLocals = true;
	}
	if(!initialized) {
		int tT = core->LoadSymbol("TRIGGER");
		int aT = core->LoadSymbol("ACTION");
		triggersTable = core->GetSymbol(tT);
		actionsTable = core->GetSymbol(aT);
		globals = new Variables();
		globals->SetType(GEM_VARIABLES_INT);
		memset(triggers, 0, MAX_TRIGGERS*sizeof(TriggerFunction));
		memset(actions, 0, MAX_ACTIONS*sizeof(ActionFunction));
		memset(blocking, 0, MAX_ACTIONS*sizeof(bool));
		triggers[0x0a] = Alignment;
		triggers[0x0b] = Allegiance;
		triggers[0x0c] = Class;
		triggers[0x0d] = Exists;
		triggers[0x0e] = General;
		triggers[0x0f] = Globals;
		triggers[0x23] = True;
		triggers[0x30] = False;
		triggers[0x36] = OnCreation;

		actions[7] = CreateCreature;
		actions[8] = Dialogue;
		actions[10] = Enemy;
		actions[22] = MoveToObject;
		blocking[22] = true;
		actions[23] = MoveToPoint;
		blocking[23] = true;
		actions[26] = PlaySound;
		actions[30] = SetGlobal;
		actions[36] = Continue;
		actions[49] = MoveViewPoint;
		actions[50] = MoveViewObject;
		actions[63] = Wait;
		blocking[63] = true;
		actions[83] = SmallWait;
		blocking[83] = true;
		actions[84] = Face;
		blocking[84] = true;
		actions[111] = DestroySelf;
		actions[120] = StartCutScene;
		actions[121] = StartCutSceneMode;
		actions[122] = EndCutSceneMode;
		actions[127] = CutSceneId;
		actions[153] = ChangeAllegiance;
		actions[154] = ChangeGeneral;
		actions[155] = ChangeRace;
		actions[156] = ChangeClass;
		actions[157] = ChangeSpecifics;
		actions[158] = ChangeGender;
		actions[159] = ChangeAlignment;
		actions[177] = TriggerActivation;
		actions[202] = FadeToColor;
		//blocking[202] = true;
		actions[203] = FadeFromColor;
		//blocking[203] = true;
		//please note that IWD and SoA are different from action #231
		actions[242] = Ally;
		if(strcmp(core->GameType, "bg2") == 0) {
			actions[254] = ScreenShake;
			blocking[254] = true;
		}
		actions[269] = DisplayStringHead;
		actions[272] = CreateVisualEffect;
		actions[273] = CreateVisualEffectObject;
		actions[286] = HideGUI;
		actions[287] = UnhideGUI;
		actions[307] = SG;
		actions[311] = DisplayStringWait;
		blocking[311] = true;
		if(strcmp(core->GameType, "pst") == 0) {
			actions[215] = FaceObject;
			actions[267] = StartSong;
		}
		initialized = 1;
	}
	continueExecution = false;
	DataStream * ds = core->GetResourceMgr()->GetResource(ResRef, IE_BCS_CLASS_ID);
	script = CacheScript(ds, ResRef);
	MySelf = NULL;
	scriptRunDelay = 1000;
	scriptType = ScriptType;
	lastRunTime = 0;
	endReached = false;
	strcpy(Name, ResRef);
}

GameScript::~GameScript(void)
{
	if(script)
		FreeScript(script);
	if(freeLocals) {
		if(locals)
			delete(locals);
	}
}

void GameScript::FreeScript(Script * script)
{
	for(int i = 0; i < script->responseBlocksCount; i++) {
		ResponseBlock * rB = script->responseBlocks[i];
		Condition * cO = rB->condition;
		for(int c = 0; c < cO->triggersCount; c++) {
			Trigger * tR = cO->triggers[c];
			if(tR->string0Parameter)
				free(tR->string0Parameter);
			if(tR->string1Parameter)
				free(tR->string1Parameter);
			if(tR->objectParameter) {
				Object * oB = tR->objectParameter;
				if(oB->objectName)
					free(oB->objectName);
				delete(oB);
			}
			delete(tR);
		}
		delete(cO->triggers);
		delete(cO);
		ResponseSet * rS = rB->responseSet;
		for(int b = 0; b < rS->responsesCount; b++) {
			Response * rP = rS->responses[b];
			for(int c = 0; c < rP->actionsCount; c++) {
				Action * aC = rP->actions[c];
				if(aC->string0Parameter)
					free(aC->string0Parameter);
				if(aC->string1Parameter)
					free(aC->string1Parameter);
				for(int c = 0; c < 3; c++) {
					Object * oB = aC->objects[c];
					if(oB) {
						if(oB->objectName)
							free(oB->objectName);
						delete(oB);
					}
				}
				delete(aC);
			}
			delete(rP->actions);
			delete(rP);
		}
		delete(rS->responses);
		delete(rS);
		delete(rB);
	}
	delete(script->responseBlocks);
	free(script->Name);
	delete(script);
}

Script* GameScript::CacheScript(DataStream * stream, const char * Context)
{
	if(!stream)
		return NULL;
	char * line = (char*)malloc(10);
	stream->ReadLine(line, 10);
	if(strncmp(line, "SC", 2) != 0) {
		printf("[IEScript]: Not a Compiled Script file\n");
		delete(stream);
		free(line);
		return NULL;
	}
	Script * newScript = new Script();
	std::vector<ResponseBlock*> rBv;
	while(true) {
		ResponseBlock * rB = ReadResponseBlock(stream);
		if(!rB)
			break;
		rBv.push_back(rB);
		stream->ReadLine(line, 10);
	}
	free(line);
	newScript->responseBlocksCount = (unsigned char)rBv.size();
	newScript->responseBlocks = new ResponseBlock*[newScript->responseBlocksCount];
	for(int i = 0; i < newScript->responseBlocksCount; i++) {
		newScript->responseBlocks[i] = rBv.at(i);
	}
	newScript->Name = (char*)malloc(9);
	strncpy(newScript->Name, Context, 8);
	delete(stream);
	return newScript;
}

void GameScript::SetVariable(const char * VarName, const char * Context, int value)
{
	char * newVarName = (char*)malloc(strlen(VarName)+strlen(Context)+1);
	strcpy(newVarName, Context);
	strcat(newVarName, VarName);
	globals->SetAt(newVarName, (unsigned long)value);
	free(newVarName);
}

void GameScript::Update()
{
	while(programmedActions.size()) {
		//printf("Executing Script Step\n");
		Action * aC = programmedActions.front();
		ExecuteAction(this, aC);
		programmedActions.pop_front();
		if(!programmedActions.size()) {
			endReached = true;
			return;
		}
		if(blocking[aC->actionID])
			return;
	}
	unsigned long thisTime;
	GetTime(thisTime);
	if((thisTime - lastRunTime) < scriptRunDelay)
		return;
	//printf("%s: Run Script\n", Name);
	lastRunTime = thisTime;
	switch(scriptType) {
		case IE_SCRIPT_TRIGGER: 
			{
			//TODO: Check if a PC is in Visible Range

			}
		break;

		case IE_SCRIPT_AREA:
			//TODO: Check if something changed in the Area
		break;
	}
	if(!script)
		return;
	for(int a = 0; a < script->responseBlocksCount; a++) {
		ResponseBlock * rB = script->responseBlocks[a];
		if(EvaluateCondition(this, rB->condition)) {
			ExecuteResponseSet(this, rB->responseSet);
			endReached = false;
			if(!continueExecution)
				break;
			continueExecution = false;	
		}
	}
}

ResponseBlock * GameScript::ReadResponseBlock(DataStream * stream)
{
	char * line = (char*)malloc(10);
	stream->ReadLine(line, 10);
	if(strncmp(line, "CR", 2) != 0) {
		free(line);
		return NULL;
	}
	free(line);
	ResponseBlock * rB = new ResponseBlock();
	rB->condition = ReadCondition(stream);
	rB->responseSet = ReadResponseSet(stream);
	return rB;
}

Condition * GameScript::ReadCondition(DataStream * stream)
{
	char * line = (char*)malloc(10);
	stream->ReadLine(line, 10);
	if(strncmp(line, "CO", 2) != 0) {
		free(line);
		return NULL;
	}
	free(line);
	Condition * cO = new Condition();
	std::vector<Trigger*> tRv;
	while(true) {
		Trigger * tR = ReadTrigger(stream);
		if(!tR)
			break;
		tRv.push_back(tR);
	}
	cO->triggersCount = (unsigned short)tRv.size();
	cO->triggers = new Trigger*[cO->triggersCount];
	for(int i = 0; i < cO->triggersCount; i++) {
		cO->triggers[i] = tRv.at(i);
	}
	return cO;
}

ResponseSet * GameScript::ReadResponseSet(DataStream * stream)
{
	char * line = (char*)malloc(10);
	stream->ReadLine(line, 10);
	if(strncmp(line, "RS", 2) != 0) {
		free(line);
		return NULL;
	}
	free(line);
	ResponseSet * rS = new ResponseSet();
	std::vector<Response*> rEv;
	while(true) {
		Response * rE = ReadResponse(stream);
		if(!rE)
			break;
		rEv.push_back(rE);
	}
	rS->responsesCount = (unsigned short)rEv.size();
	rS->responses = new Response*[rS->responsesCount];
	for(int i = 0; i < rS->responsesCount; i++) {
		rS->responses[i] = rEv.at(i);
	}
	return rS;
}

Response * GameScript::ReadResponse(DataStream * stream)
{
	char * line = (char*)malloc(1024);
	stream->ReadLine(line, 1024);
	if(strncmp(line, "RE", 2) != 0) {
		free(line);
		return NULL;
	}
	Response * rE = new Response();
	rE->weight = 0;
	int count = stream->ReadLine(line, 1024);
	for(int i = 0; i < count; i++) {
		if((line[i] >= '0') && (line[i] <= '9')) {
			rE->weight *= 10;
			rE->weight += (line[i]-'0');
			continue;
		}
		break;
	}
	std::vector<Action*> aCv;
	while(true) {
		Action * aC = new Action();
		count = stream->ReadLine(line, 1024);
		aC->actionID = 0;
		for(int i = 0; i < count; i++) {
			if((line[i] >= '0') && (line[i] <= '9')) {
				aC->actionID *= 10;
				aC->actionID += (line[i]-'0');
				continue;
			}
			break;
		}
		for(int i = 0; i < 3; i++) {
			stream->ReadLine(line, 1024);
			Object * oB = DecodeObject(line);
			aC->objects[i] = oB;
			if(i != 2)
				stream->ReadLine(line, 1024);
		}
		stream->ReadLine(line, 1024);
		aC->string0Parameter = (char*)malloc(1025);
		aC->string0Parameter[0] = 0;
		aC->string1Parameter = (char*)malloc(1025);
		aC->string1Parameter[0] = 0;
		sscanf(line, "%d %d %d %d %d\"%[^\"]\" \"%[^\"]\" AC", &aC->int0Parameter, &aC->XpointParameter, &aC->YpointParameter, &aC->int1Parameter, &aC->int2Parameter, aC->string0Parameter, aC->string1Parameter);
		aCv.push_back(aC);
		stream->ReadLine(line, 1024);
		if(strncmp(line, "RE", 2) == 0)
			break;
	}
	free(line);
	rE->actionsCount = (unsigned char)aCv.size();
	rE->actions = new Action*[rE->actionsCount];
	for(int i = 0; i < rE->actionsCount; i++) {
		rE->actions[i] = aCv.at(i);
	}
	return rE;
}

Trigger * GameScript::ReadTrigger(DataStream * stream)
{
	char * line = (char*)malloc(1024);
	stream->ReadLine(line, 1024);
	if(strncmp(line, "TR", 2) != 0) {
		free(line);
		return NULL;
	}
	stream->ReadLine(line, 1024);
	Trigger * tR = new Trigger();
	tR->string0Parameter = (char*)malloc(1025);
	tR->string0Parameter[0] = 0;
	tR->string1Parameter = (char*)malloc(1025);
	tR->string1Parameter[0] = 0;
	if(strcmp(core->GameType, "pst") == 0)
		sscanf(line, "%d %d %d %d %d [%d,%d] \"%[^\"]\" \"%[^\"]\" OB", &tR->triggerID, &tR->int0Parameter, &tR->flags, &tR->int1Parameter, &tR->int2Parameter, &tR->XpointParameter, &tR->YpointParameter, tR->string0Parameter, tR->string1Parameter);
	else
		sscanf(line, "%d %d %d %d %d \"%[^\"]\" \"%[^\"]\" OB", &tR->triggerID, &tR->int0Parameter, &tR->flags, &tR->int1Parameter, &tR->int2Parameter, tR->string0Parameter, tR->string1Parameter);
	tR->triggerID &= 0xFF;
	stream->ReadLine(line, 1024);
	tR->objectParameter = DecodeObject(line);
	stream->ReadLine(line, 1024);
	free(line);
	return tR;
}

Object * GameScript::DecodeObject(const char * line)
{
	Object * oB = new Object();
	oB->objectName = (char*)malloc(128);
	oB->objectName[0] = 0;
	if(strcmp(core->GameType, "pst") == 0)
		sscanf(line, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d [%[^]]] \"%[^\"]\"OB", &oB->eaField, &oB->factionField, &oB->teamField, &oB->generalField, &oB->raceField, &oB->classField, &oB->specificField, &oB->genderField, &oB->alignmentField, &oB->identifiersField, &oB->XobjectPosition, &oB->YobjectPosition, &oB->WobjectPosition, &oB->HobjectPosition, oB->PositionMask, oB->objectName);
	else
		sscanf(line, "%d %d %d %d %d %d %d %d %d %d %d %d \"%[^\"]\"OB", &oB->eaField, &oB->factionField, &oB->teamField, &oB->generalField, &oB->raceField, &oB->classField, &oB->specificField, &oB->genderField, &oB->alignmentField, &oB->identifiersField, &oB->XobjectPosition, &oB->YobjectPosition, oB->objectName);
	return oB;
}

bool GameScript::EvaluateCondition(GameScript * sender, Condition * condition)
{
	bool ret = true;
	for(int i = 0; i < condition->triggersCount; i++) {
		Trigger * tR = condition->triggers[i];
		ret &= EvaluateTrigger(sender, tR);
		if(!ret)
			return ret;
	}
	return ret;
}

bool GameScript::EvaluateTrigger(GameScript * sender, Trigger * trigger)
{
	TriggerFunction func = triggers[trigger->triggerID];
	if(!func) {
		//SymbolMgr * tT = core->GetSymbol(triggersTable);
		//printf("%s: Trigger not supported\n", tT->GetValue(trigger->triggerID));
		return false;
	}
	return func(sender, trigger);
}

void GameScript::ExecuteResponseSet(GameScript * sender, ResponseSet * rS)
{
	for(int i = 0; i < rS->responsesCount; i++) {
		Response * rE = rS->responses[i];
		int randWeight = (rand()%100)+1;
		if(rE->weight >= randWeight) {
			ExecuteResponse(sender, rE);
			break;
		}
	}
}

void GameScript::ExecuteResponse(GameScript * sender, Response * rE)
{
	for(int i = 0; i < rE->actionsCount; i++) {
		Action * aC = rE->actions[i];
		//ExecuteAction(sender, aC);
		programmedActions.push_back(aC);
	}
}

void GameScript::ExecuteAction(GameScript * sender, Action * aC)
{
	ActionFunction func = actions[aC->actionID];
	if(!func) {
		//SymbolMgr * aT = core->GetSymbol(actionsTable);
		//printf("%s: Action not supported\n", aT->GetValue(aC->actionID));
		return;
	}
	func(sender, aC);
}

Scriptable * GameScript::GetActorFromObject(GameScript * Sender, Object * oC)
{
	//TODO: Implement Object Retieval

	if(oC->objectName[0] != 0) {
		printf("ActionOverride on %s\n", oC->objectName);
		Map * map = core->GetGame()->GetMap(0);
		return map->GetActor(oC->objectName);
	}
	else {
		if(oC->eaField != 0) {
			switch(oC->eaField) {
				case 21:
					return core->GetGame()->GetPC(0);
				break;
			}
		}
	}
	return Sender->MySelf;
}

unsigned char GameScript::GetOrient(short sX, short sY, short dX, short dY)
{
	short deltaX = (dX-sX), deltaY = (dY-sY);
	if(deltaX > 0) {
		if(deltaY > 0) {
			return 6;
		} else if(deltaY == 0) {
			return 4;
		} else {
			return 2;
		}
	}
	else if(deltaX == 0) {
		if(deltaY > 0) {
			return 8;
		} else {
			return 0;
		}
	}
	else {
		if(deltaY > 0) {
			return 10;
		} else if(deltaY == 0) {
			return 12;
		} else {
			return 14;
		}
	}
	return 0;
}

void GameScript::ExecuteString(char * String)
{
	if(String[0] == 0)
		return;
	Action * act = GenerateAction(String);
	if(!act)
		return;
	ExecuteAction(this, act);
	if(act->string0Parameter)
		free(act->string0Parameter);
	if(act->string1Parameter)
		free(act->string1Parameter);
	for(int c = 0; c < 3; c++) {
		Object * oB = act->objects[c];
		if(oB) {
			if(oB->objectName)
				free(oB->objectName);
			delete(oB);
		}
	}
	delete(act);
	return;
}

bool GameScript::EvaluateString(char * String)
{
	if(String[0] == 0)
		return false;
	Trigger * tri = GenerateTrigger(String);
	bool ret = EvaluateTrigger(this, tri);
	if(tri->flags&1)
		ret = !ret;
	if(tri->string0Parameter)
		free(tri->string0Parameter);
	if(tri->string1Parameter)
		free(tri->string1Parameter);
	if(tri->objectParameter) {
		if(tri->objectParameter->objectName)
			free(tri->objectParameter->objectName);
		delete(tri->objectParameter);
	}
	delete(tri);
	return ret;
}

Action * GameScript::GenerateAction(char * String)
{
	Action * newAction = NULL;
	int i = 0;
	while(true) {
		char * src = String;
		char * str = actionsTable->GetStringIndex(i);
		if(!str)
			return newAction;
		while(*str) {
			if(*str != *src)
				break;
			if(*str == '(') {
				newAction = new Action();
				newAction->actionID = actionsTable->GetValueIndex(i);
				newAction->objects[0] = NULL;
				newAction->objects[1] = NULL;
				newAction->objects[2] = NULL;
				newAction->string0Parameter = NULL;
				newAction->string1Parameter = NULL;
				int objectCount = (newAction->actionID == 1) ? 0 : 1;
				int stringsCount = 0;
				int intCount = 0;
				//Here is the Trigger; Now we need to evaluate the parameters
				str++;
				src++;
				while(*str) {
					switch(*str) {
						default:
							str++;
						break;

						case 'P': //Point
							{
							while((*str != ',') && (*str != ')')) str++;
							src++; //Skip [
							char * symbol = (char*)malloc(32);
							char * tmp = symbol;
							while((*src >= '0') && (*src <= '9')) {
								*tmp = *src;
								tmp++;
								src++;
							}
							*tmp = 0;
							newAction->XpointParameter = atoi(symbol);
							src++; //Skip .
							tmp = symbol;
							while((*src >= '0') && (*src <= '9')) {
								*tmp = *src;
								tmp++;
								src++;
							}
							*tmp = 0;
							newAction->YpointParameter = atoi(symbol);
							src++; //Skip ]
							}
						break;

						case 'I': //Integer
							{
								while(*str != '*') str++;
								str++;
								SymbolMgr * valHook = NULL;
								if((*str != ',') && (*str != ')')) {
									char * idsTabName = (char*)malloc(32);
									char * tmp = idsTabName;
									while((*str != ',') && (*str != ')')) {
										*tmp = *str;
										tmp++;
										str++;
									}
									*tmp = 0;
									int i = core->LoadSymbol(idsTabName);
									valHook = core->GetSymbol(i);
									free(idsTabName);
								}
								if(!valHook) {
									char * symbol = (char*)malloc(32);
									char * tmp = symbol;
									while((*src >= '0') && (*src <= '9')) {
										*tmp = *src;
										tmp++;
										src++;
									}
									*tmp = 0;
									if(!intCount) {
										newAction->int0Parameter = atoi(symbol);
									} else if(intCount == 1) {
										newAction->int1Parameter = atoi(symbol);
									} else {
										newAction->int2Parameter = atoi(symbol);
									}
									free(symbol);
								} else {
									char * symbol = (char*)malloc(32);
									char * tmp = symbol;
									while((*src != ',') && (*src != ')')) {
										*tmp = *src;
										tmp++;
										src++;
									}
									*tmp = 0;
									if(!intCount) {
										newAction->int0Parameter = valHook->GetValue(symbol);
									} else if(intCount == 1) {
										newAction->int1Parameter = valHook->GetValue(symbol);
									} else {
										newAction->int2Parameter = valHook->GetValue(symbol);
									}
									free(symbol);
								}
							}
						break;

						case 'A': //Action
							{
								while((*str != ',') && (*str != ')')) str++;
								char *action = (char*)malloc(128);
								char *dst = action;
								int openParentesisCount = 0;
								while(true) {
									if(*src == ')') {
										if(!openParentesisCount)
											break;
										openParentesisCount--;
									} else {
										if(*src == '(') {
											openParentesisCount++;
										}
										else {
											if((*src == ',') && !openParentesisCount)
												break;
										}
									}
									*dst = *src;
									dst++;
									src++;
								}
								*dst = 0;
								Action * act = GenerateAction(action);
								free(action);
								act->objects[0] = new Object(*newAction->objects[0]);
								if(newAction->string0Parameter)
									free(newAction->string0Parameter);
								if(newAction->string1Parameter)
									free(newAction->string1Parameter);
								for(int c = 1; c < 3; c++) {
									Object * oB = newAction->objects[c];
									if(oB) {
										if(oB->objectName)
											free(oB->objectName);
										delete(oB);
									}
								}
								delete(newAction);
								newAction = act;
							}
						break;

						case 'O': //Object
							{
								while((*str != ',') && (*str != ')')) str++;
								if(*src == '"') { //Object Name
									src++;
									newAction->objects[objectCount] = new Object();
									newAction->objects[objectCount]->objectName = (char*)malloc(128);
									char *dst = newAction->objects[objectCount]->objectName;
									while(*src != '"') {
										*dst = *src;
										dst++;
										src++;
									}
									*dst = 0;
									src++;
								} else {
									
								}
								objectCount++;
							}
						break;

						case 'S': //String
							{
								while((*str != ',') && (*str != ')')) str++;
								src++;
								char * dst;
								if(!stringsCount) {
									newAction->string0Parameter = (char*)malloc(128);
									dst = newAction->string0Parameter;
								} else {
									newAction->string1Parameter = (char*)malloc(128);
									dst = newAction->string1Parameter;
								}
								while(*src != '"') {
									*dst = *src;
									dst++;
									src++;
								}
								src++;
							}
						break;
					}
					while((*src == ',') || (*src == ' '))
						src++;
				}
				return newAction;
			}
			src++;
			str++;
		}
		i++;
	}
	return newAction;
}

Trigger * GameScript::GenerateTrigger(char * String)
{
	Trigger * newTrigger = NULL;
	bool negate = false;
	if(*String == '!') {
		String++;
		negate = true;
	}
	int i = 0;
	while(true) {
		char * src = String;
		char * str = triggersTable->GetStringIndex(i);
		if(!str)
			return newTrigger;
		while(*str) {
			if(*str != *src)
				break;
			if(*str == '(') {
				newTrigger = new Trigger();
				newTrigger->triggerID = triggersTable->GetValueIndex(i);
				newTrigger->objectParameter = NULL;
				newTrigger->string0Parameter = NULL;
				newTrigger->string1Parameter = NULL;
				newTrigger->flags = (negate) ? 1 : 0;
				int stringsCount = 0;
				int intCount = 0;
				//Here is the Trigger; Now we need to evaluate the parameters
				str++;
				src++;
				while(*str) {
					switch(*str) {
						default:
							str++;
						break;

						case 'P': //Point
							{
							while((*str != ',') && (*str != ')')) str++;
							src++; //Skip [
							char * symbol = (char*)malloc(32);
							char * tmp = symbol;
							while((*src >= '0') && (*src <= '9')) {
								*tmp = *src;
								tmp++;
								src++;
							}
							*tmp = 0;
							newTrigger->XpointParameter = atoi(symbol);
							src++; //Skip .
							tmp = symbol;
							while((*src >= '0') && (*src <= '9')) {
								*tmp = *src;
								tmp++;
								src++;
							}
							*tmp = 0;
							newTrigger->YpointParameter = atoi(symbol);
							src++; //Skip ]
							}
						break;

						case 'I': //Integer
							{
								while(*str != '*') str++;
								str++;
								SymbolMgr * valHook = NULL;
								if((*str != ',') && (*str != ')')) {
									char * idsTabName = (char*)malloc(32);
									char * tmp = idsTabName;
									while((*str != ',') && (*str != ')')) {
										*tmp = *str;
										tmp++;
										str++;
									}
									*tmp = 0;
									int i = core->LoadSymbol(idsTabName);
									valHook = core->GetSymbol(i);
									free(idsTabName);
								}
								if(!valHook) {
									char * symbol = (char*)malloc(32);
									char * tmp = symbol;
									while((*src >= '0') && (*src <= '9')) {
										*tmp = *src;
										tmp++;
										src++;
									}
									*tmp = 0;
									if(!intCount) {
										newTrigger->int0Parameter = atoi(symbol);
									} else if(intCount == 1) {
										newTrigger->int1Parameter = atoi(symbol);
									} else {
										newTrigger->int2Parameter = atoi(symbol);
									}
									free(symbol);
								} else {
									char * symbol = (char*)malloc(32);
									char * tmp = symbol;
									while((*src != ',') && (*src != ')')) {
										*tmp = *src;
										tmp++;
										src++;
									}
									*tmp = 0;
									if(!intCount) {
										newTrigger->int0Parameter = valHook->GetValue(symbol);
									} else if(intCount == 1) {
										newTrigger->int1Parameter = valHook->GetValue(symbol);
									} else {
										newTrigger->int2Parameter = valHook->GetValue(symbol);
									}
									free(symbol);
								}
								intCount++;
							}
						break;

						case 'O': //Object
							{
								while((*str != ',') && (*str != ')')) str++;
								if(*src == '"') { //Object Name
									src++;
									newTrigger->objectParameter = new Object();
									newTrigger->objectParameter->objectName = (char*)malloc(128);
									char *dst = newTrigger->objectParameter->objectName;
									while(*src != '"') {
										*dst = *src;
										dst++;
										src++;
									}
									*dst = 0;
									src++;
								} else {
									
								}
							}
						break;

						case 'S': //String
							{
								while((*str != ',') && (*str != ')')) str++;
								src++;
								char * dst;
								if(!stringsCount) {
									newTrigger->string0Parameter = (char*)malloc(128);
									dst = newTrigger->string0Parameter;
								} else {
									newTrigger->string1Parameter = (char*)malloc(128);
									dst = newTrigger->string1Parameter;
								}
								while(*src != '"') {
									*dst = *src;
									dst++;
									src++;
								}
								src++;
								stringsCount++;
							}
						break;
					}
					while((*src == ',') || (*src == ' '))
						src++;
				}
				return newTrigger;
			}
			src++;
			str++;
		}
		i++;
	}
	return newTrigger;
}

//-------------------------------------------------------------
// Trigger Functions
//-------------------------------------------------------------

int GameScript::Alignment(GameScript * Sender, Trigger * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objectParameter);
	if(!scr)
		return 0;
	if(scr->Type != ST_ACTOR)
		return 0;
	Actor * actor = (Actor*)scr;
	int value = actor->GetStat(IE_ALIGNMENT);
	int a = parameters->int0Parameter&15;
	if(a) {
		if(a!=(value&15)) return 0;
	}

	a = parameters->int0Parameter&240;
	if(a) {
		if(a!=(value&240)) return 0;
	}
	return 1;
}

int GameScript::Allegiance(GameScript * Sender, Trigger * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objectParameter);
	if(!scr)
		return 0;
	if(scr->Type != ST_ACTOR)
		return 0;
	Actor * actor = (Actor*)scr;
	int value = actor->GetStat(IE_EA);
	switch(parameters->int0Parameter)
	{
	case 30: //goodcutoff
		return value<=30;
	case 31: //notgood
		return value>=31;
	case 199: //notevil
		return value<=199;
	case 200: //evilcutoff
		return value>=200;
	case 0: case 126: //anything
		return true;
	}
	return parameters->int0Parameter==value;
}

int GameScript::Class(GameScript * Sender, Trigger * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objectParameter);
	if(!scr)
		return 0;
	if(scr->Type != ST_ACTOR)
		return 0;
	Actor * actor = (Actor*)scr;
	//TODO: if parameter >=202, it is of *_ALL type
	int value = actor->GetStat(IE_CLASS);
	return parameters->int0Parameter==value;
}

int GameScript::Exists(GameScript * Sender, Trigger * parameters)
{
	Scriptable * actor = GetActorFromObject(Sender, parameters->objectParameter);
	if(actor==NULL) return 0;
	return 1;
}

int GameScript::General(GameScript * Sender, Trigger * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objectParameter);
	if(!scr)
		return 0;
	if(scr->Type != ST_ACTOR)
		return 0;
	Actor * actor = (Actor*)scr;
	if(actor==NULL) return 0;
	return parameters->int0Parameter==actor->GetStat(IE_GENERAL);
}

int GameScript::Globals(GameScript * Sender, Trigger * parameters)
{
	unsigned long value;
	if(memcmp(parameters->string0Parameter, "GLOBAL", 6) == 0) {
		if(!globals->Lookup(&parameters->string0Parameter[6], value)) 
			value = 0;
	}
	else if(memcmp(parameters->string0Parameter, "LOCALS", 6) == 0) {
		if(!Sender->locals->Lookup(&parameters->string0Parameter[6], value)) 
			value = 0;
	}
	else {
		if(!globals->Lookup(parameters->string0Parameter, value)) 
			value = 0;
	}
	int eval = (value == parameters->int0Parameter) ? 1 : 0;
	if(parameters->flags&1) {
		if(eval == 0)
			return 1;
		else
			return 0;
	}
	return eval;
}

int  GameScript::OnCreation(GameScript * Sender, Trigger * parameters)
{
	Map * area = core->GetGame()->GetMap(0);
	if(area->justCreated) {
		area->justCreated = false;
		return 1;
	}
	return 0;
}

int GameScript::True(GameScript * /* Sender*/, Trigger * /*parameters*/)
{
	return 1;
}

int GameScript::False(GameScript * /*Sender*/, Trigger * /*parameters*/)
{
	return 0;
}

//-------------------------------------------------------------
// Action Functions
//-------------------------------------------------------------

void GameScript::SG(GameScript * Sender, Action * parameters)
{
	globals->SetAt(&parameters->string0Parameter[0], parameters->int0Parameter);
}

void GameScript::SetGlobal(GameScript * Sender, Action * parameters)
{
	printf("SetGlobal(\"%s\", %d)\n", parameters->string0Parameter, parameters->int0Parameter);
	if(memcmp(parameters->string0Parameter, "GLOBAL", 6) == 0)
		globals->SetAt(&parameters->string0Parameter[6], parameters->int0Parameter);
	else if(memcmp(parameters->string0Parameter, "LOCALS", 6) == 0)
		Sender->locals->SetAt(&parameters->string0Parameter[6], parameters->int0Parameter);
	else {
		globals->SetAt(parameters->string0Parameter, parameters->int0Parameter);
	}
}

void GameScript::ChangeAllegiance(GameScript * Sender, Action * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objects[1]);
	if(!scr)
		return;
	if(scr->Type != ST_ACTOR)
		return;
	Actor * actor = (Actor*)scr;
	actor->SetStat(IE_EA,parameters->int0Parameter);
}

void GameScript::ChangeGeneral(GameScript * Sender, Action * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objects[1]);
	if(!scr)
		return;
	if(scr->Type != ST_ACTOR)
		return;
	Actor * actor = (Actor*)scr;
	actor->SetStat(IE_GENERAL,parameters->int0Parameter);
}

void GameScript::ChangeRace(GameScript * Sender, Action * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objects[1]);
	if(!scr)
		return;
	if(scr->Type != ST_ACTOR)
		return;
	Actor * actor = (Actor*)scr;
	actor->SetStat(IE_RACE,parameters->int0Parameter);
}

void GameScript::ChangeClass(GameScript * Sender, Action * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objects[1]);
	if(!scr)
		return;
	if(scr->Type != ST_ACTOR)
		return;
	Actor * actor = (Actor*)scr;
	actor->SetStat(IE_CLASS,parameters->int0Parameter);
}

void GameScript::ChangeSpecifics(GameScript * Sender, Action * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objects[1]);
	if(!scr)
		return;
	if(scr->Type != ST_ACTOR)
		return;
	Actor * actor = (Actor*)scr;
	actor->SetStat(IE_SPECIFIC,parameters->int0Parameter);
}

void GameScript::ChangeGender(GameScript * Sender, Action * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objects[1]);
	if(!scr)
		return;
	if(scr->Type != ST_ACTOR)
		return;
	Actor * actor = (Actor*)scr;
	actor->SetStat(IE_SEX,parameters->int0Parameter);
}

void GameScript::ChangeAlignment(GameScript * Sender, Action * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objects[1]);
	if(!scr)
		return;
	if(scr->Type != ST_ACTOR)
		return;
	Actor * actor = (Actor*)scr;
	actor->SetStat(IE_ALIGNMENT,parameters->int0Parameter);
}

void GameScript::TriggerActivation(GameScript * Sender, Action * parameters)
{
	InfoPoint * ip = core->GetGame()->GetMap(0)->tm->GetInfoPoint(parameters->objects[1]->objectName);
	if(!ip) {
		printf("Script error: No Trigger Named \"%s\"\n", parameters->objects[1]->objectName);
		return;
	}
	ip->Active = parameters->int0Parameter;
}

void GameScript::FadeToColor(GameScript * Sender, Action * parameters)
{
	core->timer->SetFadeToColor(parameters->XpointParameter);
}

void GameScript::FadeFromColor(GameScript * Sender, Action * parameters)
{
	core->timer->SetFadeFromColor(parameters->XpointParameter);
}

void GameScript::CreateCreature(GameScript * Sender, Action * parameters)
{
	ActorMgr * aM = (ActorMgr*)core->GetInterface(IE_CRE_CLASS_ID);
	DataStream * ds = core->GetResourceMgr()->GetResource(parameters->string0Parameter, IE_CRE_CLASS_ID);
	aM->Open(ds, true);
	Actor *ab = aM->GetActor();
	ab->MoveTo(parameters->XpointParameter, parameters->YpointParameter);
	ab->AnimID = IE_ANI_AWAKE;
	ab->Orientation = parameters->int0Parameter;
	Map * map = core->GetGame()->GetMap(0);
	map->AddActor(ab);
	core->FreeInterface(aM);
}

void GameScript::StartCutSceneMode(GameScript * Sender, Action * parameters)
{
	core->SetCutSceneMode(true);
}

void GameScript::EndCutSceneMode(GameScript * Sender, Action * parameters)
{
	core->SetCutSceneMode(false);
}

void GameScript::StartCutScene(GameScript * Sender, Action * parameters)
{
	GameScript * gs = new GameScript(parameters->string0Parameter, IE_SCRIPT_ALWAYS);
	core->timer->SetCutScene(gs);
}

void GameScript::CutSceneId(GameScript * Sender, Action * parameters)
{
	if(parameters->objects[0]->genderField != 0) {
		Sender->MySelf = GetActorFromObject(Sender, parameters->objects[0]);
	} else {
		Map * map = core->GetGame()->GetMap(0);
		Sender->MySelf = map->GetActor(parameters->objects[1]->objectName);
	}
}

void GameScript::Enemy(GameScript * Sender, Action * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objects[0]);
	if(!scr)
		return;
	if(scr->Type != ST_ACTOR)
		return;
	Actor * actor = (Actor*)scr;
	actor->SetStat(IE_EA,255);
}

void GameScript::Ally(GameScript * Sender, Action * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objects[0]);
	if(!scr)
		return;
	if(scr->Type != ST_ACTOR)
		return;
	Actor * actor = (Actor*)scr;
	actor->SetStat(IE_EA,4);
}

void GameScript::Wait(GameScript * Sender, Action * parameters)
{
	core->timer->SetWait(parameters->int0Parameter*AI_UPDATE_TIME);
}

void GameScript::SmallWait(GameScript * Sender, Action * parameters)
{
	core->timer->SetWait(parameters->int0Parameter);
}

void GameScript::MoveViewPoint(GameScript * Sender, Action * parameters)
{
	core->GetVideoDriver()->MoveViewportTo(parameters->XpointParameter, parameters->YpointParameter);
}

void GameScript::MoveViewObject(GameScript * Sender, Action * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objects[1]);
	if(scr->Type != ST_ACTOR)
		return;
	Actor * actor = (Actor*)scr;
	if(actor) {
		core->GetVideoDriver()->MoveViewportTo(actor->XPos, actor->YPos);
	}
}

void GameScript::MoveToPoint(GameScript * Sender, Action * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objects[0]);
	if(!scr)
		return;
	if(scr->Type != ST_ACTOR)
		return;
	Actor * actor = (Actor*)scr;
	if(actor) {
		actor->WalkTo(parameters->XpointParameter, parameters->YpointParameter);
		core->timer->SetMovingActor(actor);
	}
}

void GameScript::MoveToObject(GameScript * Sender, Action * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objects[0]);
	if(!scr)
		return;
	Scriptable * target = GetActorFromObject(Sender, parameters->objects[1]);
	if(!target)
		return;
	if(scr->Type != ST_ACTOR)
		return;
	Actor * actor = (Actor*)scr;
	actor->WalkTo(target->XPos, target->YPos);
	core->timer->SetMovingActor(actor);
}

void GameScript::DisplayStringHead(GameScript * Sender, Action * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objects[0]);
	if(!scr)
		return;
	if(scr->Type != ST_ACTOR)
		return;
	Actor * actor = (Actor*)scr;
	if(actor) {
		printf("Displaying string on: %s\n", actor->scriptName);
		actor->DisplayHeadText(core->GetString(parameters->int0Parameter,2));
	}
}

void GameScript::Face(GameScript * Sender, Action * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objects[0]);
	if(!scr)
		return;
	if(scr->Type != ST_ACTOR)
		return;
	Actor * actor = (Actor*)scr;
	if(actor) {
		actor->Orientation = parameters->int0Parameter;
	}
}

void GameScript::FaceObject(GameScript * Sender, Action * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objects[0]);
	if(!scr)
		return;
	Scriptable * target = GetActorFromObject(Sender, parameters->objects[1]);
	if(!target)
		return;
	if(scr->Type != ST_ACTOR)
		return;
	Actor * actor = (Actor*)scr;
	actor->Orientation = GetOrient(target->XPos, target->YPos, actor->XPos, actor->YPos);
}

void GameScript::DisplayStringWait(GameScript * Sender, Action * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objects[0]);
	if(!scr)
		return;
	if(scr->Type != ST_ACTOR)
		return;
	Actor * actor = (Actor*)scr;
	printf("Displaying string on: %s\n", actor->scriptName);
	StringBlock sb = core->strings->GetStringBlock(parameters->int0Parameter);
	actor->DisplayHeadText(sb.text);
	if(sb.Sound[0]) {
		unsigned long len = core->GetSoundMgr()->Play(sb.Sound);
		if(len != 0xffffffff)
			//waitCounter = ((15*len)/1000);
			core->timer->SetWait((AI_UPDATE_TIME*len)/1000);
	}
}

void GameScript::StartSong(GameScript * Sender, Action * parameters)
{
	int MusicTable = core->LoadTable("music");
	if(MusicTable>=0) {
		TableMgr * music = core->GetTable(MusicTable);
		char * string = music->QueryField(parameters->int0Parameter, 0);
		if(string[0] == '*') {
			core->GetMusicMgr()->HardEnd();
		}
		else {
			core->GetMusicMgr()->SwitchPlayList(string, true);
		}
	}
}

void GameScript::Continue(GameScript * Sender, Action * parameters)
{
	Sender->continueExecution = true;
}

void GameScript::PlaySound(GameScript * Sender, Action * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objects[0]);
	if(scr) {
		core->GetSoundMgr()->Play(parameters->string0Parameter);
	}
}

void GameScript::CreateVisualEffectObject(GameScript * Sender, Action * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objects[1]);
	if(!scr)
		return;
	if(scr->Type != ST_ACTOR)
		return;
	Actor * target = (Actor*)scr;
	DataStream * ds = core->GetResourceMgr()->GetResource(parameters->string0Parameter, IE_VVC_CLASS_ID);
	ScriptedAnimation * vvc = new ScriptedAnimation(ds, true, target->XPos, target->YPos);
	core->GetGame()->GetMap(0)->AddVVCCell(vvc);
}

void GameScript::CreateVisualEffect(GameScript * Sender, Action * parameters)
{
	DataStream * ds = core->GetResourceMgr()->GetResource(parameters->string0Parameter, IE_VVC_CLASS_ID);
	ScriptedAnimation * vvc = new ScriptedAnimation(ds, true, parameters->XpointParameter, parameters->YpointParameter);
	core->GetGame()->GetMap(0)->AddVVCCell(vvc);
}

void GameScript::DestroySelf(GameScript * Sender, Action * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objects[0]);
	if(!scr)
		return;
	if(scr->Type != ST_ACTOR)
		return;
	Actor * actor = (Actor*)scr;
	actor->DeleteMe = true;
}

void GameScript::ScreenShake(GameScript * Sender, Action * parameters)
{
	core->timer->SetScreenShake(parameters->XpointParameter, parameters->YpointParameter, parameters->int0Parameter);
}

void GameScript::UnhideGUI(GameScript * Sender, Action * parameters)
{
	GameControl * gc = (GameControl*)core->GetWindow(0)->GetControl(0);	
	if(gc->ControlType == IE_GUI_GAMECONTROL)
		gc->UnhideGUI();
	EndCutSceneMode(Sender, parameters);
}

void GameScript::HideGUI(GameScript * Sender, Action * parameters)
{
	GameControl * gc = (GameControl*)core->GetWindow(0)->GetControl(0);	
	if(gc->ControlType == IE_GUI_GAMECONTROL)
		gc->HideGUI();
}

void GameScript::Dialogue(GameScript * Sender, Action * parameters)
{
	Scriptable * scr = GetActorFromObject(Sender, parameters->objects[0]);
	if(!scr)
		return;
	Scriptable * tar = GetActorFromObject(Sender, parameters->objects[1]);
	if(!tar)
		return;
	if(scr->Type != ST_ACTOR)
		return;
	Actor * actor = (Actor*)scr;
	Actor * target = (Actor*)tar;
	if(actor->Dialog[0] != 0) {
		DialogMgr * dm = (DialogMgr*)core->GetInterface(IE_DLG_CLASS_ID);
		dm->Open(core->GetResourceMgr()->GetResource(actor->Dialog, IE_DLG_CLASS_ID), true);
		GameControl * gc = (GameControl*)core->GetWindow(0)->GetControl(0);	
		if(gc->ControlType == IE_GUI_GAMECONTROL)
			gc->InitDialog(actor, target, dm->GetDialog());
		core->FreeInterface(dm);
	}
}
