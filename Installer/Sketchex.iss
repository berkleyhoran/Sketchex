; Inno Setup script for Sketchex -- "fruity aero" (sky-blue/glass) wizard,
; same structure/palette as gexex's sibling plugins (Synthex, Reactex).
; Build with: ISCC.exe Installer\Sketchex.iss   (after a Release build)
; Produces Installer\Output\SketchexSetup.exe -- a single installer
; anyone can run: it puts the VST3 where every DAW looks for it
; (C:\Program Files\Common Files\VST3) and optionally installs the
; Standalone app with a Start Menu shortcut.

#define AppName "Sketchex"
; Overridable via `ISCC.exe /DAppVersion=1.0.1 Sketchex.iss` -- CI passes
; the git tag's version through this way instead of editing the file.
; Local/manual builds (no /D flag) just get this default.
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#define BuildDir "..\build\Sketchex_artefacts\Release"

[Setup]
; Unique to Sketchex -- Inno uses AppId (not AppName) to identify a product
; across updates, so this must never collide with Synthex/Reactex.
AppId={{A0FE222D-C27E-403A-BE1A-E39645418B80}
AppName={#AppName}
AppVersion={#AppVersion}
; gexex-family product: publishes under the gexex name (matches COMPANY_NAME
; in CMakeLists.txt).
AppPublisher=gexex
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
OutputBaseFilename=SketchexSetup
OutputDir=Output
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
; Installing into Common Files\VST3 needs admin rights.
PrivilegesRequired=admin
WizardStyle=modern
WizardSizePercent=110
SetupIconFile=..\Assets\logo.ico
WizardImageFile=wizard.bmp
WizardSmallImageFile=wizard_small.bmp
WizardImageStretch=yes

[Types]
Name: "full"; Description: "VST3 plugin + Standalone app"
Name: "vst3only"; Description: "VST3 plugin only"
Name: "custom"; Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plugin (for Ableton/any DAW)"; Types: full vst3only custom; Flags: fixed
Name: "standalone"; Description: "Standalone app"; Types: full custom

[Files]
; The .vst3 is a bundle folder -- copy it whole into the system VST3 dir.
Source: "{#BuildDir}\VST3\Sketchex.vst3\*"; DestDir: "{commoncf64}\VST3\Sketchex.vst3"; \
    Components: vst3; Flags: recursesubdirs ignoreversion
Source: "{#BuildDir}\Standalone\Sketchex.exe"; DestDir: "{app}"; \
    Components: standalone; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\Sketchex.exe"; Components: standalone
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\Sketchex.exe"; Description: "Launch {#AppName}"; \
    Components: standalone; Flags: nowait postinstall skipifsilent

[Messages]
WelcomeLabel1=Welcome to [name]
WelcomeLabel2=Draw your melody. A gexex MIDI sequencer -- sketch a line on the canvas and it plays as scale-locked notes into any synth after it.%n%nThis will install the [name] VST3 plugin (and optionally the standalone app) on your computer.%n%nDrop it in front of a synth, draw, press play.
FinishedHeadingLabel=You're in.
FinishedLabel=[name] is installed. In your DAW, rescan plugins if it doesn't show up right away (Ableton: Preferences > Plug-Ins > Rescan).

[Code]
// "Fruity aero": a light sky-blue/glass palette, not a dark theme --
// Inno's default "modern" wizard style is already fairly light, but this
// pulls in the plugin's own actual brand colours (the same hex values
// GexexLookAndFeel.cpp uses for its ColourScheme) rather than leaving it
// generic, so the installer reads as part of the same product. Inno's
// TColor is Delphi's format ($00BBGGRR -- blue/green/red, reversed from
// the usual RRGGBB web convention), so every constant below is the
// BGR-swapped form of the plugin's own ARGB hex.
procedure ApplyAeroTheme(Parent: TWinControl);
var
  I: Integer;
  C: TControl;
begin
  for I := 0 to Parent.ControlCount - 1 do
  begin
    C := Parent.Controls[I];
    if C is TNewStaticText then
    begin
      TNewStaticText(C).Font.Color := $00493016; // ink navy (GexexLookAndFeel::inkColour, BGR)
      TNewStaticText(C).Color := $00FFFEFB;      // near-white glass panel fill
    end
    else if C is TLabel then
    begin
      TLabel(C).Font.Color := $00493016;
      TLabel(C).Transparent := True;
    end
    else if C is TNewCheckListBox then
    begin
      TNewCheckListBox(C).Color := $00FBEFDC;    // light-blue glass fill
      TNewCheckListBox(C).Font.Color := $00493016;
    end
    else if C is TNewEdit then
    begin
      TNewEdit(C).Color := $00FFFEFB;
      TNewEdit(C).Font.Color := $00493016;
    end
    else if C is TNewMemo then
    begin
      TNewMemo(C).Color := $00FFFEFB;
      TNewMemo(C).Font.Color := $00493016;
    end
    else if C is TWinControl then
    begin
      ApplyAeroTheme(TWinControl(C));
    end;
  end;
end;

procedure InitializeWizard();
begin
  // BGR hex: sky-blue chrome, matching the plugin's own background gradient.
  WizardForm.Color := $00FFF6EA;
  WizardForm.MainPanel.Color := $00FFF6EA;
  WizardForm.InnerPage.Color := $00FFFEFB;
  WizardForm.TasksList.Color := $00FBEFDC;
  WizardForm.PageNameLabel.Font.Color := $006E3BFF;   // candy-pink accent (GexexLookAndFeel's tick/heading pink)
  WizardForm.PageDescriptionLabel.Font.Color := $00493016;
  WizardForm.Bevel.Visible := False;
  WizardForm.Bevel1.Visible := False;
  WizardForm.WelcomePage.Color := $00FFF6EA;
  WizardForm.FinishedPage.Color := $00FFF6EA;
  WizardForm.WelcomeLabel1.Font.Color := $006E3BFF;
  WizardForm.WelcomeLabel1.Font.Size := 14;
  WizardForm.WelcomeLabel1.Font.Style := [fsBold];
  WizardForm.WelcomeLabel2.Font.Color := $00493016;
  WizardForm.FinishedHeadingLabel.Font.Color := $006E3BFF;
  WizardForm.FinishedHeadingLabel.Font.Size := 14;
  WizardForm.FinishedLabel.Font.Color := $00493016;
  ApplyAeroTheme(WizardForm.InnerPage);
end;
