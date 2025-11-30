' RigidLabeler Launcher
' This script starts the backend server and frontend application
' without showing command prompt windows

Option Explicit

Dim WshShell, fso, scriptDir, backendPath, frontendPath

Set WshShell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")

' Get the directory where this script is located
scriptDir = fso.GetParentFolderName(WScript.ScriptFullName)

' Build paths
backendPath = scriptDir & "\backend\rigidlabeler_backend\rigidlabeler_backend.exe"
frontendPath = scriptDir & "\frontend\frontend.exe"

' Start backend server (hidden)
If fso.FileExists(backendPath) Then
    WshShell.Run Chr(34) & backendPath & Chr(34), 0, False
Else
    MsgBox "Backend not found: " & backendPath, vbCritical, "RigidLabeler Error"
    WScript.Quit 1
End If

' Wait for backend to initialize
WScript.Sleep 2000

' Start frontend
If fso.FileExists(frontendPath) Then
    WshShell.Run Chr(34) & frontendPath & Chr(34), 1, False
Else
    MsgBox "Frontend not found: " & frontendPath, vbCritical, "RigidLabeler Error"
    WScript.Quit 1
End If

Set WshShell = Nothing
Set fso = Nothing
