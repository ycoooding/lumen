!include "LogicLib.nsh"
!include "nsDialogs.nsh"
!include "WinMessages.nsh"

Var LumenMachineCode
Var LumenExpectedActivation
Var LumenActivationInput
Var LumenMachineCodeField
Var LumenActivationField
Var LumenActivationStatus
Var LumenActivationVerified
Var LumenNextButton

Function LumenLoadActivationData
  StrCmp $LumenMachineCode "" 0 done

  InitPluginsDir
  File /oname=$PLUGINSDIR\lumen-installer-activation.ps1 "${LUMEN_ACTIVATION_SCRIPT}"
  nsExec::ExecToStack /TIMEOUT=20000 \
    'powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "$PLUGINSDIR\lumen-installer-activation.ps1"'
  Pop $0
  Pop $1

  StrCmp $0 "0" 0 failed
  StrLen $2 $1
  IntCmp $2 55 0 failed failed
  StrCpy $2 $1 1 27
  StrCmp $2 "|" 0 failed

  StrCpy $LumenMachineCode $1 27 0
  StrCpy $LumenExpectedActivation $1 27 28
  Goto done

failed:
  StrCpy $LumenMachineCode "ERROR"
  StrCpy $LumenExpectedActivation ""

done:
FunctionEnd

Function LumenActivationPageCreate
  !insertmacro MUI_HEADER_TEXT "Lumen 安装授权" "输入与本机机器码匹配的激活码"

  Call LumenLoadActivationData

  nsDialogs::Create 1018
  Pop $0

  ${NSD_CreateLabel} 0 0 100% 18u "机器码（选中后可按 Ctrl+C 复制）："
  Pop $0
  ${NSD_CreateText} 0 18u 100% 13u "$LumenMachineCode"
  Pop $LumenMachineCodeField
  SendMessage $LumenMachineCodeField ${EM_SETREADONLY} 1 0

  ${NSD_CreateLabel} 0 48u 100% 18u "激活码："
  Pop $0
  ${NSD_CreateText} 0 66u 100% 13u ""
  Pop $LumenActivationField
  ${NSD_OnChange} $LumenActivationField LumenActivationChanged

  ${NSD_CreateLabel} 0 92u 100% 26u "把机器码发送给授权方，收到激活码后粘贴到上方。"
  Pop $LumenActivationStatus

  GetDlgItem $LumenNextButton $HWNDPARENT 1
  EnableWindow $LumenNextButton 0

  StrCmp $LumenExpectedActivation "" 0 show
    ${NSD_SetText} $LumenActivationStatus "无法读取本机物理硬件信息，安装不能继续。"

show:
  nsDialogs::Show
FunctionEnd

Function LumenActivationChanged
  ${NSD_GetText} $LumenActivationField $LumenActivationInput
  StrCpy $LumenActivationVerified "0"
  EnableWindow $LumenNextButton 0

  StrCmp $LumenExpectedActivation "" invalid
  StrCmp $LumenActivationInput $LumenExpectedActivation 0 invalid
    StrCpy $LumenActivationVerified "1"
    EnableWindow $LumenNextButton 1
    ${NSD_SetText} $LumenActivationStatus "激活码正确，可以继续安装。"
    Return

invalid:
  ${NSD_SetText} $LumenActivationStatus "激活码不正确。"
FunctionEnd

Function LumenActivationPageLeave
  ${NSD_GetText} $LumenActivationField $LumenActivationInput
  StrCmp $LumenActivationVerified "1" 0 denied
  StrCmp $LumenActivationInput $LumenExpectedActivation 0 denied
  Return

denied:
  MessageBox MB_OK|MB_ICONSTOP "激活码不正确，无法继续安装。"
  Abort
FunctionEnd
