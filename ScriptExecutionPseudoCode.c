in Script condition checking and execute function, after condition execute and float return
check if returned value is bigger or equal to ScriptConditionCheck unkFloat3
pseudo code:
	if scriptState & 0x400 == 0 {
		ScriptSwitchBody = scriptState.switchBody
	}
	fVar2 = -1000000015047466219876688855040.00000000
	ScriprConditionCheck = NULL
	switchBody = NULL
	action = NULL
	do {
		if ScriptSwitchBody == NULL and switchBody == NULL {
			return 0
		}
		if ScriptSwitchBody == NULL and switchBody != NULL {
			if scriptState.scriptIndex != -1 {
				UnkFun()
			}
			UnkFun()
			Type5.switchBody = NULL
			action = switchBody.action
			switchBodyBitfield = switchBody.bitfield
			while (action != NULL) {
				action.execute(unkStruct_0xC, unkScriptRelatedStruct2, Type5)
				action = action.nextAction
			}
			if switchBodyBitfield & 0x100 != 0 {
				Type5.field_0x18_bitfield |= 0x200
			}
			Type5.field_0x14 = unkStruct_0xC.field_0x4
			unkScriptState = switchBody.state
			if unkScriptState.Type1 != NULL {
				// Unknown class methods call
			}
			Type5.scriptState = unkScriptState
			unkScriptState2 = NULL
			if scriptState != unkScriptState {
				unkScriptState2 = scriptState
			}
			Type5.field_0x18_bitfield |= 0x200
			Type5.scriptState2 = unkScriptState2
			return 1
		}
		fVar8 = ScriptConditionCheck.checkCondition(objectInstance?, Type5, unkInt)
		fVar9 = fVar2
		unkSwitchBody2 = switchBody
		if fVar8 >= ScriptConditionCheck.unkFloat3 {
			fVar7 = ScriprConditionCheck.unkFloat4
			if (ScriptConditionCheck.unkFloat1 & 0x10000) == 0x0 {
				fVar9 = fVar8 - ScriptConditionCheck.unkFloat3
			} else {
				fVar9 = ScriptConditionCheck.unkFloat3 - fVar8
			}
			fVar9 *= fVar7
			unkSwitchBody2 = ScriptSwitchBody
			if (fVar9 <= fVar2) {
				fVar9 = fVar2
				unkSwitchBody2 = switchBody
			}
		}
		ScriptSwitchBody = ScriptSwitchBody.nextSwitchBody
		fVar2 = fVar9
		switchBody = unkSwitchBody2
	} while (true)