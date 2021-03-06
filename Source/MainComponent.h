/*
 This file is part of SpatGRIS2.
 
 Developers: Olivier Belanger, Nicolas Masson
 
 SpatGRIS2 is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 SpatGRIS2 is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with SpatGRIS2.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef MAINCOMPONENT_H_INCLUDED
#define MAINCOMPONENT_H_INCLUDED

// Macros
#ifndef M_PI
#define M_PI    (3.14159265358979323846264338327950288)
#endif

#ifndef M2_PI
#define M2_PI   (6.28318530717958647692528676655900577)
#endif

#ifndef M_PI2
#define M_PI2   (1.57079632679489661923132169163975143)
#endif

#ifndef M_PI4
#define M_PI4   (0.785398163397448309615660845819875720)
#endif

#define STRING2(x) #x
#define STRING(x) STRING2(x)

//============================

#include "../JuceLibraryCode/JuceHeader.h"

#include "GrisLookAndFeel.h"
#include "ServerGrisConstants.h"
#include "jackClientGRIS.h"
#include "jackServerGRIS.h"
#include "Speaker.h"
#include "SpeakerViewComponent.h"
#include "UiComponent.h"
#include "LevelComponent.h"
#include "OscInput.h"
#include "Input.h"
#include "WinControl.h"
#include "MainWindow.h"

using namespace std;

// This component lives inside our window, and this is where you should put all your controls and content.
class MainContentComponent : public Component,
                             public MenuBarModel,
                             public ApplicationCommandTarget,
                             public Button::Listener,
                             public TextEditor::Listener,
                             public Slider::Listener,
                             public ComboBox::Listener,
                             private Timer
{
public:
    MainContentComponent(DocumentWindow *parent);
    ~MainContentComponent();

    // Exit application.
    bool isPresetModified();
    bool exitApp();

    // Menubar handlers.
    void handleNew();
    void handleOpenPreset();
    void handleSavePreset();
    void handleSaveAsPreset();
    void handleOpenSpeakerSetup();
    void handleSaveAsSpeakerSetup(); // Called when closing the Speaker Setup Edition window.
    void handleShowSpeakerEditWindow();
    void handleShowPreferences();
    void handleShowAbout();
    void handleOpenManual();
    void handleShow2DView();
    void handleShowNumbers();
    void setShowNumbers(bool state);
    void handleShowSpeakers();
    void setShowSpeakers(bool state);
    void handleShowTriplets();
    void setShowTriplets(bool state);
    bool validateShowTriplets();
    void handleShowSourceLevel();
    void handleShowSpeakerLevel();
    void handleShowSphere();
    void handleResetInputPositions();
    void handleResetMeterClipping();
    void handleShowOscLogView();
    void handleInputColours();

    // Menubar methods.
    StringArray getMenuBarNames() override {
        const char* const names[] = { "File", "View", "Help", nullptr };
        return StringArray (names);
    }
    PopupMenu getMenuForIndex (int menuIndex, const String& /*menuName*/) override;
    void menuItemSelected (int menuItemID, int /*topLevelMenuIndex*/) override;

    // Speakers.
    vector<Speaker *> getListSpeaker() { return this->listSpeaker; }
    mutex* getLockSpeakers() { return this->lockSpeakers; }
    Speaker * getSpeakerFromOutputPatch(int out);
    void addSpeaker(int sortColumnId = 1, bool isSortedForwards = true);
    void insertSpeaker(int position, int sortColumnId, bool isSortedForwards);
    void removeSpeaker(int idSpeaker);
    void setDirectOut(int id, int chn);
    void reorderSpeakers(vector<int> newOrder);
    void resetSpeakerIds();
    int getMaxSpeakerId();
    int getMaxSpeakerOutputPatch();

    // Sources.
    vector<Input *> getListSourceInput() { return this->listSourceInput; }
    mutex* getLockInputs() { return this->lockInputs; }
    void updateInputJack(int inInput, Input &inp);
    bool isRadiusNormalized();

    // Jack clients.
    jackClientGris * getJackClient() { return this->jackClient; }
    mutex* getLockClients() { return &this->jackClient->lockListClient; }
    vector<Client> *getListClientjack() { return &this->jackClient->listClient; }
    void connectionClientJack(String nameCli, bool conn = true);

    // VBAP triplets.
    void setListTripletFromVbap();
    vector<Triplet> getListTriplet() { return this->listTriplet; }
    void clearListTriplet() { this->listTriplet.clear(); }

    // Speaker selections.
    void selectSpeaker(unsigned int idS);
    void selectTripletSpeaker(int idS);
    bool tripletExist(Triplet tri, int &pos);

    // Mute - solo.
    void muteInput(int id, bool mute);
    void muteOutput(int id, bool mute);
    void soloInput(int id, bool solo);
    void soloOutput(int id, bool solo);

    // Input - output amplitude levels.
    float getLevelsOut(int indexLevel) { return (20.0f * log10f(this->jackClient->getLevelsOut(indexLevel))); }
    float getLevelsIn(int indexLevel) { return (20.0f * log10f(this->jackClient->getLevelsIn(indexLevel))); }
    float getLevelsAlpha(int indexLevel) {
        float level = this->jackClient->getLevelsIn(indexLevel);
        if (level > 0.0001) { // -80 dB
            return 1.0;
        } else {
            return sqrtf(level * 10000.0f);
        }
    }
    float getSpeakerLevelsAlpha(int indexLevel) {
        float level = this->jackClient->getLevelsOut(indexLevel);
        float alpha = 1.0;
        if (level > 0.001) { // -60 dB
            alpha = 1.0;
        } else {
            alpha = sqrtf(level * 1000.0f);
        }
        if (alpha < 0.6) {
            alpha = 0.6;
        }
        return alpha;
    }

    // Called when the speaker setup has changed.
    bool updateLevelComp();

    // Open - save.
    void openXmlFileSpeaker(String path);
    void reloadXmlFileSpeaker();
    void openPreset(String path);
    void getPresetData(XmlElement *xml);
    void savePreset(String path);
    void saveSpeakerSetup(String path);
    void saveProperties(String device, int rate, int buff, int fileformat, int fileconfig, int attenuationDB, int attenuationHz, int oscPort);
    void chooseRecordingPath();
    void setNameConfig();
    void setTitle();

    // Screen refresh timer.
    void handleTimer(bool state) {
        if (state) {
            startTimerHz(24);
        } else {
            stopTimer();
        }
    }

    // Close windows other than the main one.
    void destroyWinSpeakConf() { this->winSpeakConfig = nullptr; this->jackClient->processBlockOn = true; }
    void destroyWindowProperties() { this->windowProperties = nullptr; }
    void destroyWinControl() { this->winControlSource = nullptr; }
    void destroyAboutWindow() { this->aboutWindow = nullptr; }
    void destroyOscLogWindow() { this->oscLogWindow = nullptr; }

    // Widget listener handlers.
    void timerCallback() override;
    void paint(Graphics& g) override;
    void resized() override;
    void buttonClicked(Button *button) override;
    void sliderValueChanged(Slider *slider) override;
    void textEditorFocusLost(TextEditor &textEditor) override;
    void textEditorReturnKeyPressed(TextEditor &textEditor) override;
    void comboBoxChanged(ComboBox *comboBox) override;

    int getModeSelected();

    void setOscLogging(const OSCMessage& message);

    // App user settings.
    ApplicationProperties applicationProperties;
    int oscInputPort = 18032;
    unsigned int samplingRate = 48000;
    juce::Rectangle<int> winControlRect;

    // Visual flags.
    bool isSourceLevelShown;
    bool isSpeakerLevelShown;
    bool isTripletsShown;
    bool isSpanShown;

    // App states.
    bool needToSavePreset = false;
    bool needToSaveSpeakerSetup = false;
    bool needToComputeVbap = true;

    // Widget creation helper.
    TextEditor* addTextEditor(const String &s, const String &emptyS, const String &stooltip,
                              int x, int y, int w, int h, Component *into, int wLab = 80);

private:

    // Look-and-feel.
    GrisLookAndFeel mGrisFeel;
    SmallGrisLookAndFeel mSmallTextGrisFeel;

    DocumentWindow *parent;

    std::unique_ptr<MenuBarComponent> menuBar;

    // Widget creation helpers.
    Label *        addLabel(const String &s, const String &stooltip, int x, int y, int w, int h, Component *into);
    TextButton *   addButton(const String &s, const String &stooltip, int x, int y, int w, int h, Component *into);
    ToggleButton * addToggleButton(const String &s, const String &stooltip, int x, int y, int w, int h, Component *into, bool toggle = false);
    Slider *       addSlider(const String &s, const String &stooltip, int x, int y, int w, int h, Component *into);
    ComboBox *     addComboBox(const String &s, const String &stooltip, int x, int y, int w, int h, Component *into);

    // Jack server - client.
    jackServerGRIS *jackServer;
    jackClientGris *jackClient;

    // Speakers.
    vector<Triplet>   listTriplet;
    vector<Speaker *> listSpeaker;
    mutex             *lockSpeakers;

    // Sources.
    vector<Input *> listSourceInput;
    mutex           *lockInputs;

    // Open Sound Control.
    OscInput *oscReceiver;

    // Paths.
    String nameConfig;
    String pathLastVbapSpeakerSetup;
    String pathCurrentFileSpeaker;
    String pathCurrentPreset;

    // Alsa output device
    String alsaOutputDevice;
    Array<String> alsaAvailableOutputDevices;

    // UI Components.
    SpeakerViewComponent *speakerView;
    StretchableLayoutManager verticalLayout;
    std::unique_ptr<StretchableLayoutResizerBar> verticalDividerBar;

    // Windows.
    WindowEditSpeaker *winSpeakConfig;
    WindowProperties *windowProperties;
    WinControl *winControlSource;
    AboutWindow *aboutWindow;
    OscLogWindow *oscLogWindow;

    // 3 Main Boxes.
    Box *boxMainUI;
    Box *boxInputsUI;
    Box *boxOutputsUI;
    Box *boxControlUI;
    
    // Component in Box 3.
    Label *labelJackStatus;
    Label *labelJackLoad;
    Label *labelJackRate;
    Label *labelJackBuffer;
    Label *labelJackInfo;

    ComboBox *comBoxModeSpat;

    Slider *sliderMasterGainOut;
    Slider *sliderInterpolation;
    
    TextEditor *tedAddInputs;
        
    Label *labelAllClients;
    BoxClient *boxClientJack;
    
    TextButton *butStartRecord;
    TextEditor *tedMinRecord;
    Label *labelTimeRecorded;
    TextButton *butInitRecord;

    // App splash screen.
    SplashScreen *splash;

    // Flags.
    bool isProcessForeground;
    bool isNumbersShown;
    bool isSpeakersShown;
    bool isSphereShown;
    bool isRecording;

    // The following methods implement the ApplicationCommandTarget interface, allowing
    // this window to publish a set of actions it can perform, and which can be mapped
    // onto menus, keypresses, etc.
    ApplicationCommandTarget* getNextCommandTarget() override { return findFirstTargetParentComponent(); }
    void getAllCommands(Array<CommandID>& commands) override;
    void getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) override;
    bool perform(const InvocationInfo& info) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};

#endif  // MAINCOMPONENT_H_INCLUDED
