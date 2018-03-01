/*
 This file is part of ServerGris.
 
 Developers: Nicolas Masson
 
 ServerGris is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 ServerGris is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with ServerGris.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "UiComponent.h"
#include "LevelComponent.h"
#include "MainComponent.h"

static double GetFloatPrecision(double value, double precision)
{
    return (floor((value * pow(10, precision) + 0.5)) / pow(10, precision));
}

//======================================= BOX ===========================================================================
Box::Box(GrisLookAndFeel *feel, String title, bool verticalScrollbar, bool horizontalScrollbar)
{
    this->title = title;
    this->grisFeel = feel;
    this->bgColour = this->grisFeel->getBackgroundColour();
    
    this->content = new Component();
    this->viewport = new Viewport();
    this->viewport->setViewedComponent(this->content, false);
    this->viewport->setScrollBarsShown(verticalScrollbar, horizontalScrollbar);
    this->viewport->setScrollBarThickness(6);
    this->viewport->getVerticalScrollBar()->setColour(ScrollBar::ColourIds::thumbColourId, feel->getScrollBarColour());
    this->viewport->getHorizontalScrollBar()->setColour(ScrollBar::ColourIds::thumbColourId, feel->getScrollBarColour());
    
    this->viewport->setLookAndFeel(this->grisFeel);
    addAndMakeVisible(this->viewport);
}

Box::~Box()
{
    this->content->deleteAllChildren();
    delete this->viewport;
    delete this->content;
}

Component * Box::getContent()
{
    return this->content ? this->content : this;
}


void Box::resized()
{
    if (this->viewport){
        this->viewport->setSize(getWidth(), getHeight());
    }
}

void Box::correctSize(unsigned int width, unsigned int height)
{
    if(this->title!=""){
        this->viewport->setTopLeftPosition(0, 20);
        this->viewport->setSize(getWidth(), getHeight()-20);
        if(width<80){
            width = 80;
        }
    }else{
        this->viewport->setTopLeftPosition(0, 0);
    }
    this->getContent()->setSize(width, height);
}


void Box::paint(Graphics &g)
{
    g.setColour(this->bgColour);
    g.fillRect(getLocalBounds());
    if(this->title!=""){
        g.setColour (this->grisFeel->getWinBackgroundColour());
        g.fillRect(0,0,getWidth(),18);
        
        g.setColour (this->grisFeel->getFontColour());
        g.drawText(title, 0, 0, this->content->getWidth(), 20, juce::Justification::left);
    }
}




//======================================= BOX CLIENT ===========================================================================
BoxClient::BoxClient(MainContentComponent * parent, GrisLookAndFeel *feel)
{
    this->mainParent= parent;
    this->grisFeel = feel;
    
    tableListClient.setModel(this);
    
    tableListClient.setColour (ListBox::outlineColourId, this->grisFeel->getWinBackgroundColour());
    tableListClient.setColour(ListBox::backgroundColourId, this->grisFeel->getWinBackgroundColour());
    tableListClient.setOutlineThickness (1);
    
    tableListClient.getHeader().addColumn("Client",    1, 105, 70, 120, TableHeaderComponent::notSortable);
    tableListClient.getHeader().addColumn("Start",     2, 45, 35, 70, TableHeaderComponent::notSortable);
    tableListClient.getHeader().addColumn("End",       3, 45, 35, 70, TableHeaderComponent::notSortable);
    tableListClient.getHeader().addColumn("Available", 4, 62, 35, 70, TableHeaderComponent::notSortable);
    tableListClient.getHeader().addColumn("On/Off",    5, 41, 35, 70, TableHeaderComponent::notSortable);


    tableListClient.setMultipleSelectionEnabled (false);
    
    numRows = 0;
    tableListClient.updateContent();
    
    this->addAndMakeVisible(tableListClient);
}

BoxClient::~BoxClient(){

}

void BoxClient::buttonClicked(Button *button)
{
    this->mainParent->getLockClients()->lock();
    bool connectedCli = !this->mainParent->getListClientjack()->at(button->getName().getIntValue()).connected;
    this->mainParent->connectionClientJack(this->mainParent->getListClientjack()->at(button->getName().getIntValue()).name, connectedCli);
    updateContentCli();
    this->mainParent->getLockClients()->unlock();
}

void BoxClient::setBounds(int x, int y, int width, int height)
{
    this->juce::Component::setBounds(x, y, width, height);
    tableListClient.setSize(width, height);
}

void BoxClient::updateContentCli()
{
    numRows = (unsigned int)this->mainParent->getListClientjack()->size();
    tableListClient.updateContent();
    tableListClient.repaint();
}

void BoxClient::setValue (const int rowNumber, const int columnNumber,const int newRating)
{
    this->mainParent->getLockClients()->lock();
    if (this->mainParent->getListClientjack()->size()> rowNumber)
    {
        switch(columnNumber){
            case 2 :
                this->mainParent->getListClientjack()->at(rowNumber).portStart = newRating;
                this->mainParent->getListClientjack()->at(rowNumber).initialized = true;
                break;
            case 3 :
                this->mainParent->getListClientjack()->at(rowNumber).portEnd= newRating;
                this->mainParent->getListClientjack()->at(rowNumber).initialized = true;
                break;
        }
    }
    this->mainParent->getLockClients()->unlock();
}

int BoxClient::getValue (const int rowNumber,const int columnNumber) const{
    if (this->mainParent->getListClientjack()->size()> rowNumber)
    {
        switch(columnNumber){
                
            case 2 :
                
                return this->mainParent->getListClientjack()->at(rowNumber).portStart;
                break;
            case 3 :
                return this->mainParent->getListClientjack()->at(rowNumber).portEnd;
                break;
                
        }
    }
    
    return -1;
}

String BoxClient::getText (const int columnNumber, const int rowNumber) const
{
    String text = "?";

    if (this->mainParent->getListClientjack()->size()> rowNumber)
    {
        
        switch(columnNumber){
            case 1 :
                text =String(this->mainParent->getListClientjack()->at(rowNumber).name);
                break;
            case 4 :
                text =String(this->mainParent->getListClientjack()->at(rowNumber).portAvailable);
                break;
                
        }
    }

    return text;
}

int BoxClient::getNumRows()
{
    return numRows;
}

void BoxClient::paintRowBackground (Graphics& g, int rowNumber, int /*width*/, int /*height*/, bool rowIsSelected)
{
    /*if (rowIsSelected){
        g.fillAll (this->grisFeel->getOnColour());
    }
    else{*/
        if (rowNumber % 2){
            g.fillAll (this->grisFeel->getBackgroundColour().withBrightness(0.6));
        }else{
            g.fillAll (this->grisFeel->getBackgroundColour().withBrightness(0.7));
        }
    //}
}


void BoxClient::paintCell (Graphics& g, int rowNumber, int columnId, int width, int height, bool /*rowIsSelected*/)
{
    g.setColour (Colours::black);
    g.setFont (12.0f);
    if(this->mainParent->getLockClients()->try_lock()){
    if (this->mainParent->getListClientjack()->size()> rowNumber)
    {
        if(columnId==1){
            String text = getText(columnId, rowNumber);
            g.drawText (text, 2, 0, width - 4, height, Justification::centredLeft, true);
        }
        if(columnId==4){
            String text = getText(columnId, rowNumber);
            g.drawText (text, 2, 0, width - 4, height, Justification::centred, true);
        }
    }
    this->mainParent->getLockClients()->unlock();
    }
    g.setColour (Colours::black.withAlpha (0.2f));
    g.fillRect (width - 1, 0, 1, height);
}

Component* BoxClient::refreshComponentForCell (int rowNumber, int columnId, bool /*isRowSelected*/,
                                                       Component* existingComponentToUpdate)
{
     if(columnId==1|| columnId==4){
         return existingComponentToUpdate;
     }
    
    if(columnId==5){
        TextButton* tbRemove = static_cast<TextButton*> (existingComponentToUpdate);
        if (tbRemove == nullptr){
            tbRemove = new TextButton ();
            tbRemove->setName(String(rowNumber));
            tbRemove->setBounds(4, 404, 88, 22);
            tbRemove->addListener(this);
            tbRemove->setColour(ToggleButton::textColourId, this->grisFeel->getFontColour());
            tbRemove->setLookAndFeel(this->grisFeel);
        }

        if(this->mainParent->getListClientjack()->at(rowNumber).connected){
            tbRemove->setButtonText("<->");
        }else{
            tbRemove->setButtonText("<X>");
        }

                //tbRemove->getContent()->addAndMakeVisible(this->butAddSpeaker);
        return tbRemove;
    }

    ListIntOutComp* textLabel = static_cast<ListIntOutComp*> (existingComponentToUpdate);
    
    // same as above...
    if (textLabel == nullptr)
        textLabel = new ListIntOutComp (*this);
    
    textLabel->setRowAndColumn (rowNumber, columnId);
    
    return textLabel;
}



//======================================= Window Edit Speaker============================================================

WindowEditSpeaker::WindowEditSpeaker(const String& name, String& nameC, Colour backgroundColour, int buttonsNeeded,
                                     MainContentComponent * parent, GrisLookAndFeel * feel):
    DocumentWindow (name, backgroundColour, buttonsNeeded), font (14.0f)
{
    this->mainParent = parent;
    this->grisFeel = feel;
    
    this->boxListSpeaker = new Box(this->grisFeel, "Configuration Speakers");
    
    this->butAddSpeaker = new TextButton();
    this->butAddSpeaker->setButtonText("Add Speaker");
    this->butAddSpeaker->setBounds(5, 404, 100, 22);
    this->butAddSpeaker->addListener(this);
    this->butAddSpeaker->setColour(ToggleButton::textColourId, this->grisFeel->getFontColour());
    this->butAddSpeaker->setLookAndFeel(this->grisFeel);
    this->boxListSpeaker->getContent()->addAndMakeVisible(this->butAddSpeaker);

    this->butcompSpeakers = new TextButton();
    this->butcompSpeakers->setButtonText("Compute");
    this->butcompSpeakers->setBounds(110, 404, 100, 22);
    this->butcompSpeakers->addListener(this);
    this->butcompSpeakers->setColour(ToggleButton::textColourId, this->grisFeel->getFontColour());
    this->butcompSpeakers->setLookAndFeel(this->grisFeel);
    this->boxListSpeaker->getContent()->addAndMakeVisible(this->butcompSpeakers);

    /* Generate ring of speakers */
    int wlab = 80;

    this->rNumOfSpeakersLabel = new Label();
    this->rNumOfSpeakersLabel->setText("# of speakers", NotificationType::dontSendNotification);
    this->rNumOfSpeakersLabel->setJustificationType(Justification::right);
    this->rNumOfSpeakersLabel->setFont(this->grisFeel->getFont());
    this->rNumOfSpeakersLabel->setLookAndFeel(this->grisFeel);
    this->rNumOfSpeakersLabel->setColour(Label::textColourId, this->grisFeel->getFontColour());
    this->rNumOfSpeakersLabel->setBounds(5, 435, 40, 24);
    this->boxListSpeaker->getContent()->addAndMakeVisible(this->rNumOfSpeakersLabel);

    this->rNumOfSpeakers = new TextEditor();
    this->rNumOfSpeakers->setTooltip("Number of speakers in the ring");
    this->rNumOfSpeakers->setColour(ToggleButton::textColourId, this->grisFeel->getFontColour());
    this->rNumOfSpeakers->setLookAndFeel(this->grisFeel);
    this->rNumOfSpeakers->setBounds(5+wlab, 435, 40, 24);
    this->rNumOfSpeakers->addListener(this->mainParent);
    this->boxListSpeaker->getContent()->addAndMakeVisible(this->rNumOfSpeakers);
    this->rNumOfSpeakers->setText("8");
    this->rNumOfSpeakers->setInputRestrictions(3, "0123456789");
    this->rNumOfSpeakers->addListener(this);

    this->rZenithLabel = new Label();
    this->rZenithLabel->setText("Zenith", NotificationType::dontSendNotification);
    this->rZenithLabel->setJustificationType(Justification::right);
    this->rZenithLabel->setFont(this->grisFeel->getFont());
    this->rZenithLabel->setLookAndFeel(this->grisFeel);
    this->rZenithLabel->setColour(Label::textColourId, this->grisFeel->getFontColour());
    this->rZenithLabel->setBounds(105, 435, 80, 24);
    this->boxListSpeaker->getContent()->addAndMakeVisible(this->rZenithLabel);

    this->rZenith = new TextEditor();
    this->rZenith->setTooltip("Elevation angle of the ring");
    this->rZenith->setColour(ToggleButton::textColourId, this->grisFeel->getFontColour());
    this->rZenith->setLookAndFeel(this->grisFeel);
    this->rZenith->setBounds(105+wlab, 435, 60, 24);
    this->rZenith->addListener(this->mainParent);
    this->boxListSpeaker->getContent()->addAndMakeVisible(this->rZenith);
    this->rZenith->setText("0.0");
    this->rZenith->setInputRestrictions(6, "-0123456789.");
    this->rZenith->addListener(this);

    this->rRadiusLabel = new Label();
    this->rRadiusLabel->setText("Radius", NotificationType::dontSendNotification);
    this->rRadiusLabel->setJustificationType(Justification::right);
    this->rRadiusLabel->setFont(this->grisFeel->getFont());
    this->rRadiusLabel->setLookAndFeel(this->grisFeel);
    this->rRadiusLabel->setColour(Label::textColourId, this->grisFeel->getFontColour());
    this->rRadiusLabel->setBounds(230, 435, 80, 24);
    this->boxListSpeaker->getContent()->addAndMakeVisible(this->rRadiusLabel);

    this->rRadius = new TextEditor();
    this->rRadius->setTooltip("Distance of the speakers from the center.");
    this->rRadius->setColour(ToggleButton::textColourId, this->grisFeel->getFontColour());
    this->rRadius->setLookAndFeel(this->grisFeel);
    this->rRadius->setBounds(230+wlab, 435, 60, 24);
    this->rRadius->addListener(this->mainParent);
    this->boxListSpeaker->getContent()->addAndMakeVisible(this->rRadius);
    this->rRadius->setText("1.0");
    this->rRadius->setInputRestrictions(6, "0123456789.");
    this->rRadius->addListener(this);

    this->rOffsetAngleLabel = new Label();
    this->rOffsetAngleLabel->setText("Offset Angle", NotificationType::dontSendNotification);
    this->rOffsetAngleLabel->setJustificationType(Justification::right);
    this->rOffsetAngleLabel->setFont(this->grisFeel->getFont());
    this->rOffsetAngleLabel->setLookAndFeel(this->grisFeel);
    this->rOffsetAngleLabel->setColour(Label::textColourId, this->grisFeel->getFontColour());
    this->rOffsetAngleLabel->setBounds(375, 435, 80, 24);
    this->boxListSpeaker->getContent()->addAndMakeVisible(this->rOffsetAngleLabel);

    this->rOffsetAngle = new TextEditor();
    this->rOffsetAngle->setTooltip("Offset angle of the first speaker.");
    this->rOffsetAngle->setColour(ToggleButton::textColourId, this->grisFeel->getFontColour());
    this->rOffsetAngle->setLookAndFeel(this->grisFeel);
    this->rOffsetAngle->setBounds(375+wlab, 435, 60, 24);
    this->rOffsetAngle->addListener(this->mainParent);
    this->boxListSpeaker->getContent()->addAndMakeVisible(this->rOffsetAngle);
    this->rOffsetAngle->setText("0.0");
    this->rOffsetAngle->setInputRestrictions(6, "-0123456789.");
    this->rOffsetAngle->addListener(this);

    this->butAddRing = new TextButton();
    this->butAddRing->setButtonText("Add Ring");
    this->butAddRing->setBounds(520, 435, 100, 24);
    this->butAddRing->addListener(this);
    this->butAddRing->setColour(ToggleButton::textColourId, this->grisFeel->getFontColour());
    this->butAddRing->setLookAndFeel(this->grisFeel);
    this->boxListSpeaker->getContent()->addAndMakeVisible(this->butAddRing);

    /* End generate ring of speakers */

    /* Pink noise controls */
    this->pinkNoise = new ToggleButton();
    this->pinkNoise->setButtonText("Reference Pink Noise");
    this->pinkNoise->setBounds(5, 500, 150, 24);
    this->pinkNoise->addListener(this);
    this->pinkNoise->setColour(ToggleButton::textColourId, this->grisFeel->getFontColour());
    this->pinkNoise->setLookAndFeel(this->grisFeel);
    this->boxListSpeaker->getContent()->addAndMakeVisible(this->pinkNoise);

    this->pinkNoiseGain = new Slider();
    this->pinkNoiseGain->setTextValueSuffix(" dB");
    this->pinkNoiseGain->setBounds(200, 500, 60, 60);
    this->pinkNoiseGain->setSliderStyle(Slider::Rotary);
    this->pinkNoiseGain->setRotaryParameters(M_PI * 1.3f, M_PI * 2.7f, true);
    this->pinkNoiseGain->setRange(-60, -3, 1);
    this->pinkNoiseGain->setValue(-20);
    this->pinkNoiseGain->setTextBoxStyle (Slider::TextBoxBelow, false, 60, 20);
    this->pinkNoiseGain->setColour(ToggleButton::textColourId, this->grisFeel->getFontColour());
    this->pinkNoiseGain->setLookAndFeel(this->grisFeel);
    this->pinkNoiseGain->addListener(this);
    this->boxListSpeaker->getContent()->addAndMakeVisible(this->pinkNoiseGain);

    /* End pink noise controls */

    this->setContentOwned(this->boxListSpeaker, false);
    this->boxListSpeaker->getContent()->addAndMakeVisible(tableListSpeakers);

    this->boxListSpeaker->repaint();
    this->boxListSpeaker->resized();
}

WindowEditSpeaker::~WindowEditSpeaker()
{
    delete this->butAddSpeaker;
    delete this->butcompSpeakers;
    delete this->rNumOfSpeakers;
    delete this->rNumOfSpeakersLabel;
    delete this->rZenith;
    delete this->rZenithLabel;
    delete this->rRadius;
    delete this->rRadiusLabel;
    delete this->rOffsetAngle;
    delete this->rOffsetAngleLabel;
    delete this->butAddRing;
    delete this->pinkNoise;
    delete this->pinkNoiseGain;
    this->mainParent->destroyWinSpeakConf();
}

void WindowEditSpeaker::initComp()
{
    tableListSpeakers.setModel(this);
    
    tableListSpeakers.setColour (ListBox::outlineColourId, this->grisFeel->getWinBackgroundColour());
    tableListSpeakers.setColour(ListBox::backgroundColourId, this->grisFeel->getWinBackgroundColour());
    tableListSpeakers.setOutlineThickness (1);
    
    tableListSpeakers.getHeader().addColumn("ID", 1, 40, 40, 60,TableHeaderComponent::defaultFlags);
    tableListSpeakers.getHeader().addColumn("X", 2, 70, 50, 120,TableHeaderComponent::defaultFlags);
    tableListSpeakers.getHeader().addColumn("Y", 3, 70, 50, 120,TableHeaderComponent::defaultFlags);
    tableListSpeakers.getHeader().addColumn("Z", 4, 70, 50, 120,TableHeaderComponent::defaultFlags);
    tableListSpeakers.getHeader().addColumn("Azimuth", 5, 70, 50, 120,TableHeaderComponent::defaultFlags);
    tableListSpeakers.getHeader().addColumn("Zenith", 6, 70, 50, 120,TableHeaderComponent::defaultFlags);
    tableListSpeakers.getHeader().addColumn("Radius", 7, 70, 50, 120,TableHeaderComponent::defaultFlags);
    tableListSpeakers.getHeader().addColumn("Output", 8, 70, 50, 120,TableHeaderComponent::defaultFlags);
    tableListSpeakers.getHeader().addColumn("Gain (dB)", 9, 70, 50, 120,TableHeaderComponent::defaultFlags);
    tableListSpeakers.getHeader().addColumn("Highpass", 10, 70, 50, 120,TableHeaderComponent::defaultFlags);
    tableListSpeakers.getHeader().addColumn("Direct", 11, 70, 50, 120,TableHeaderComponent::defaultFlags);
    tableListSpeakers.getHeader().addColumn("delete", 12, 70, 50, 120,TableHeaderComponent::defaultFlags);
    
    tableListSpeakers.getHeader().setSortColumnId (1, true); // sort forwards by the ID column
    
    tableListSpeakers.setMultipleSelectionEnabled (false);
    
    numRows = (unsigned int)this->mainParent->getListSpeaker().size();

    this->boxListSpeaker->setBounds(0, 0, getWidth(),getHeight());
    this->boxListSpeaker->correctSize(getWidth()-8, getHeight());
    tableListSpeakers.setSize(getWidth(), 400);
    
    tableListSpeakers.updateContent();
    
    this->boxListSpeaker->repaint();
    this->boxListSpeaker->resized();
    this->resized();
}

//void WindowEditSpeaker::sortOrderChanged(int newSortColumnId, bool isForwards) {
//    tableListSpeakers.getHeader().setSortColumnId(newSortColumnId, isForwards);
//   tableListSpeakers.getHeader().reSortTable();
    //tableListSpeakers.updateContent();
//}

void WindowEditSpeaker::sliderValueChanged (Slider *slider) {
    float gain;
    if (slider == this->pinkNoiseGain) {
        gain = powf(10.0f, this->pinkNoiseGain->getValue() / 20.0f);
        this->mainParent->getJackClient()->pinkNoiseGain = gain;
    }
}

void WindowEditSpeaker::buttonClicked(Button *button)
{
    bool tripletState = this->mainParent->isTripletsShown;
    this->mainParent->setShowTriplets(false);
    if (button == this->butAddSpeaker) {
        this->mainParent->addSpeaker();
        updateWinContent();
        this->mainParent->needToComputeVbap = true;
        this->tableListSpeakers.selectRow(this->getNumRows()-1);
    }
    else if (button == this->butcompSpeakers) {
        if (this->mainParent->updateLevelComp()) {
            if (tripletState) {
                this->mainParent->setShowTriplets(true);
            }
        }
    }
    else if (button == this->butAddRing) {
        for (int i = 0; i < this->rNumOfSpeakers->getText().getIntValue(); i++) {
            this->mainParent->addSpeaker();
            float azimuth = 360.0f / this->rNumOfSpeakers->getText().getIntValue() * i + this->rOffsetAngle->getText().getFloatValue();
            if (azimuth > 360.0f) {
                azimuth -= 360.0f;
            } else if (azimuth < 0.0f) {
                azimuth += 360.0f;
            }
            float zenith = this->rZenith->getText().getFloatValue();
            float radius = this->rRadius->getText().getFloatValue();
            this->mainParent->getListSpeaker().back()->setAziZenRad(glm::vec3(azimuth, zenith, radius));
            
        }
        updateWinContent();
        this->mainParent->needToComputeVbap = true;
        this->tableListSpeakers.selectRow(this->getNumRows()-1);
    }
    else if (button == this->pinkNoise) {
        this->mainParent->getJackClient()->noiseSound = this->pinkNoise->getToggleState();
    }
    else if (button->getName() != "" && (button->getName().getIntValue() >= 0 &&
        button->getName().getIntValue() <= this->mainParent->getListSpeaker().size())) {
        this->mainParent->removeSpeaker(button->getName().getIntValue());
        updateWinContent();
        this->mainParent->needToComputeVbap = true;
    }
    else {
        int row = button->getName().getIntValue() - 1000;
        if (button->getToggleState()) {
            this->mainParent->getListSpeaker()[row]->setDirectOut(true);
        }
        else {
            this->mainParent->getListSpeaker()[row]->setDirectOut(false);
        }
        updateWinContent();
        this->mainParent->needToComputeVbap = true;
    }
}

void WindowEditSpeaker::textEditorTextChanged(TextEditor& editor) {
    float value;
    String test;
    if (&editor == this->rNumOfSpeakers) {} 
    else if (&editor == this->rZenith) {
        test = this->rZenith->getText().retainCharacters(".");
        if (test.length() > 1) {
            this->rZenith->setText(this->rZenith->getText().dropLastCharacters(1), false);
        }
        value = this->rZenith->getText().getFloatValue();
        if (value > 90.0f) {
            this->rZenith->setText(String(90.0f), false);
        } else if (value < -90.0f) {
            this->rZenith->setText(String(-90.0f), false);
        }
    } else if (&editor == this->rRadius) {
        test = this->rRadius->getText().retainCharacters(".");
        if (test.length() > 1) {
            this->rRadius->setText(this->rRadius->getText().dropLastCharacters(1), false);
        }
        value = this->rRadius->getText().getFloatValue();
        if (value > 1.0f) {
            this->rRadius->setText(String(1.0f), false);
        }
    } else if (&editor == this->rOffsetAngle) {
        test = this->rOffsetAngle->getText().retainCharacters(".");
        if (test.length() > 1) {
            this->rOffsetAngle->setText(this->rOffsetAngle->getText().dropLastCharacters(1), false);
        }
        value = this->rOffsetAngle->getText().getFloatValue();
        if (value < -180.0f) {
            this->rOffsetAngle->setText(String(-180.0f), false);
        } else if (value > 180.0f) {
            this->rOffsetAngle->setText(String(180.0f), false);
        }
    }
}

void WindowEditSpeaker::textEditorReturnKeyPressed(TextEditor& editor) {
    this->unfocusAllComponents();
}

void WindowEditSpeaker::updateWinContent()
{
    this->numRows = (unsigned int)this->mainParent->getListSpeaker().size();
    this->tableListSpeakers.updateContent();
    this->mainParent->needToSaveSpeakerSetup = true;
}

void WindowEditSpeaker::selectedRow(int value)
{
    MessageManagerLock mmlock;
    this->tableListSpeakers.selectRow(value);
    this->repaint();
}

void WindowEditSpeaker::closeButtonPressed()
{
    int exitV = 1;
    if (this->mainParent->needToSaveSpeakerSetup) {
        exitV = AlertWindow::showYesNoCancelBox(AlertWindow::WarningIcon,
                                                "Closing Speaker Setup Window !",
                                                "Do you want to compute and save the current setup ?");
        if (exitV == 1) {
            this->mainParent->updateLevelComp();
            this->mainParent->handleTimer(false);
            this->setAlwaysOnTop(false);
            this->mainParent->handleSaveAsSpeakerSetup();
            this->mainParent->handleTimer(true);
        }
    }
    if (exitV) {
        this->mainParent->getJackClient()->noiseSound = false;
        delete this;
    }
}

void WindowEditSpeaker::resized()
{
    this->juce::DocumentWindow::resized();

    tableListSpeakers.setSize(getWidth(), getHeight()-195);
    
    this->boxListSpeaker->setSize(getWidth(), getHeight());
    this->boxListSpeaker->correctSize(getWidth()-10, getHeight()-30);

    this->butAddSpeaker->setBounds(5, getHeight()-180, 100, 22);
    this->butcompSpeakers->setBounds(getWidth()-105, getHeight()-180, 100, 22);

    this->rNumOfSpeakersLabel->setBounds(5, getHeight()-140, 80, 24);
    this->rNumOfSpeakers->setBounds(5+80, getHeight()-140, 40, 24);
    this->rZenithLabel->setBounds(100, getHeight()-140, 80, 24);
    this->rZenith->setBounds(100+80, getHeight()-140, 60, 24);
    this->rRadiusLabel->setBounds(215, getHeight()-140, 80, 24);
    this->rRadius->setBounds(215+80, getHeight()-140, 60, 24);
    this->rOffsetAngleLabel->setBounds(360, getHeight()-140, 80, 24);
    this->rOffsetAngle->setBounds(360+80, getHeight()-140, 60, 24);
    this->butAddRing->setBounds(getWidth()-105, getHeight()-140, 100, 24);

    this->pinkNoise->setBounds(5, getHeight()-75, 150, 24);
    this->pinkNoiseGain->setBounds(180, getHeight()-100, 60, 60);
}

String WindowEditSpeaker::getText (const int columnNumber, const int rowNumber) const
{
    String text = "";
    //this->mainParent->getLockSpeakers()->lock();
    if (this->mainParent->getListSpeaker().size() > rowNumber) {
        switch(columnNumber){
            case 1 :
                text = String(this->mainParent->getListSpeaker()[rowNumber]->getIdSpeaker());
                break;
            case 2 :
                text = String(this->mainParent->getListSpeaker()[rowNumber]->getCoordinate().x);
                break;
            case 3 :
                text = String(this->mainParent->getListSpeaker()[rowNumber]->getCoordinate().z);
                break;
            case 4 :
                text = String(this->mainParent->getListSpeaker()[rowNumber]->getCoordinate().y);
                break;
            case 5 :
                text = String(this->mainParent->getListSpeaker()[rowNumber]->getAziZenRad().x);
                break;
            case 6 :
                text = String(this->mainParent->getListSpeaker()[rowNumber]->getAziZenRad().y);
                break;
            case 7 :
                text = String(this->mainParent->getListSpeaker()[rowNumber]->getAziZenRad().z);
                break;
            case 8 :
                text = String(this->mainParent->getListSpeaker()[rowNumber]->getOutputPatch());
                break;
            case 9:
                text = String(this->mainParent->getListSpeaker()[rowNumber]->getGain());
                break;
            case 10:
                text = String(this->mainParent->getListSpeaker()[rowNumber]->getHighPassCutoff());
                break;
            case 11:
                text = String(this->mainParent->getListSpeaker()[rowNumber]->getDirectOut());
                break;
            default:
                text = "?";
        }
    }

    //this->mainParent->getLockSpeakers()->unlock();
    return text;
}

void WindowEditSpeaker::setText(const int columnNumber, const int rowNumber, const String& newText)
{
    int ival, oldval;
    float val;
    if(this->mainParent->getLockSpeakers()->try_lock()) {
        if (this->mainParent->getListSpeaker().size() > rowNumber) {
            glm::vec3 newP;
            switch(columnNumber) {
                case 2 :
                    newP = this->mainParent->getListSpeaker()[rowNumber]->getCoordinate();
                    newP.x = GetFloatPrecision(newText.getFloatValue(), 3);
                    this->mainParent->getListSpeaker()[rowNumber]->setCoordinate(newP);
                    break;
                case 3 :
                    newP = this->mainParent->getListSpeaker()[rowNumber]->getCoordinate();
                    newP.z = GetFloatPrecision(newText.getFloatValue(), 3);
                    this->mainParent->getListSpeaker()[rowNumber]->setCoordinate(newP);
                    break;
                case 4 :
                    newP = this->mainParent->getListSpeaker()[rowNumber]->getCoordinate();
                    newP.y = GetFloatPrecision(newText.getFloatValue(), 3);
                    this->mainParent->getListSpeaker()[rowNumber]->setCoordinate(newP);
                    break;
                case 5 :
                    newP = this->mainParent->getListSpeaker()[rowNumber]->getAziZenRad();
                    newP.x = GetFloatPrecision(newText.getFloatValue(), 2);
                    this->mainParent->getListSpeaker()[rowNumber]->setAziZenRad(newP);
                    break;
                case 6 :
                    newP = this->mainParent->getListSpeaker()[rowNumber]->getAziZenRad();
                    val = GetFloatPrecision(newText.getFloatValue(), 2);
                    if (val < -90.0f) { val = -90.0f; }
                    else if (val > 90.0f) { val = 90.0f; }
                    newP.y = val;
                    this->mainParent->getListSpeaker()[rowNumber]->setAziZenRad(newP);
                    break;
                case 7 :
                    newP = this->mainParent->getListSpeaker()[rowNumber]->getAziZenRad();
                    newP.z = GetFloatPrecision(newText.getFloatValue(), 2);
                    this->mainParent->getListSpeaker()[rowNumber]->setAziZenRad(newP);
                    break;
                case 8:
                    this->mainParent->setShowTriplets(false);
                    oldval = this->mainParent->getListSpeaker()[rowNumber]->getOutputPatch();
                    ival = newText.getIntValue();
                    if (ival < 0) {
                        ival = 0;
                    } else if (ival > 256) {
                        ival = 256;
                    }
                    for (auto&& it : this->mainParent->getListSpeaker()) {
                        if (it->getOutputPatch() == ival) {
                            ScopedPointer<AlertWindow> alert = new AlertWindow ("Wrong output patch!    ",
                                                                                "Sorry! Output patch number " + String(ival) + " is already used.", 
                                                                                AlertWindow::WarningIcon);
                            alert->setLookAndFeel(this->grisFeel);
                            alert->addButton ("OK", 0);
                            alert->runModalLoop();
                            ival = oldval;
                        }
                    }
                    this->mainParent->getListSpeaker()[rowNumber]->setOutputPatch(ival);
                    break;
                case 9 :
                    val = newText.getFloatValue();
                    if (val < -18.0f) { val = -18.0f; }
                    else if (val > 6.0f) { val = 6.0f; }
                    this->mainParent->getListSpeaker()[rowNumber]->setGain(val);
                    break;
                case 10 :
                    val = newText.getFloatValue();
                    if (val < 0.0f) { val = 0.0f; }
                    else if (val > 150.0f) { val = 150.0f; }
                    this->mainParent->getListSpeaker()[rowNumber]->setHighPassCutoff(val);
                    break;
                case 11 :
                    this->mainParent->setShowTriplets(false);
                    this->mainParent->getListSpeaker()[rowNumber]->setDirectOut(newText.getIntValue());
                    break;
            }
        }
        this->updateWinContent();
        this->mainParent->needToComputeVbap = true;
        this->mainParent->getLockSpeakers()->unlock();
    }
}

int WindowEditSpeaker::getNumRows()
{
    return numRows;
}

// This is overloaded from TableListBoxModel, and should fill in the background of the whole row
void WindowEditSpeaker::paintRowBackground (Graphics& g, int rowNumber, int /*width*/, int /*height*/, bool rowIsSelected)
{
    if (rowIsSelected){
        if(this->mainParent->getLockSpeakers()->try_lock()){
        this->mainParent->getListSpeaker()[rowNumber]->selectSpeaker();
        this->mainParent->getLockSpeakers()->unlock();
        }
        g.fillAll (this->grisFeel->getHighlightColour());
    }
    else{
        if(this->mainParent->getLockSpeakers()->try_lock()){
        this->mainParent->getListSpeaker()[rowNumber]->unSelectSpeaker();
        this->mainParent->getLockSpeakers()->unlock();
        }
        if (rowNumber % 2){
            g.fillAll (this->grisFeel->getBackgroundColour().withBrightness(0.6));
        }else{
            g.fillAll (this->grisFeel->getBackgroundColour().withBrightness(0.7));
        }
    }
}

// This is overloaded from TableListBoxModel, and must paint any cells that aren't using custom
// components.
void WindowEditSpeaker::paintCell(Graphics& g, int rowNumber, int columnId, int width, int height, bool /*rowIsSelected*/)
{
    g.setColour (Colours::black);
    g.setFont (font);
    
    if (this->mainParent->getLockSpeakers()->try_lock()) {
        if (this->mainParent->getListSpeaker().size() > rowNumber) {
            String text = getText(columnId, rowNumber);
            g.drawText(text, 2, 0, width - 4, height, Justification::centredLeft, true);
        }
        this->mainParent->getLockSpeakers()->unlock();
    }
    g.setColour (Colours::black.withAlpha (0.2f));
    g.fillRect (width - 1, 0, 1, height);
}

Component* WindowEditSpeaker::refreshComponentForCell(int rowNumber, int columnId, bool /*isRowSelected*/,
                                                      Component* existingComponentToUpdate)
{
    if (columnId == 11){
        ToggleButton* tbDirect = static_cast<ToggleButton*> (existingComponentToUpdate);
        if (tbDirect == nullptr)
            tbDirect = new ToggleButton ();
        tbDirect->setName(String(rowNumber  + 1000));
        tbDirect->setClickingTogglesState(true);
        tbDirect->setBounds(4, 404, 88, 22);
        tbDirect->addListener(this);
        tbDirect->setToggleState(this->mainParent->getListSpeaker()[rowNumber]->getDirectOut(), dontSendNotification);
        tbDirect->setLookAndFeel(this->grisFeel);
        return tbDirect;
    }
    if (columnId == 12){
        TextButton* tbRemove = static_cast<TextButton*> (existingComponentToUpdate);
        if (tbRemove == nullptr)
            tbRemove = new TextButton ();
        tbRemove->setButtonText("X");
        tbRemove->setName(String(rowNumber));
        tbRemove->setBounds(4, 404, 88, 22);
        tbRemove->addListener(this);
        tbRemove->setColour(ToggleButton::textColourId, this->grisFeel->getFontColour());
        tbRemove->setLookAndFeel(this->grisFeel);
        return tbRemove;
    }
    // The other columns are editable text columns, for which we use the custom Label component
    EditableTextCustomComponent* textLabel = static_cast<EditableTextCustomComponent*> (existingComponentToUpdate);
    if (textLabel == nullptr)
        textLabel = new EditableTextCustomComponent (*this);
    
    textLabel->setRowAndColumn (rowNumber, columnId);
    if(columnId==1){    //ID Speakers
        textLabel->setEditable(false);
    }
    return textLabel;
}




//======================================= WindowProperties ===========================
Label *
WindowProperties::createPropLabel(String lab, Justification::Flags just, int ypos) {
    Label *label = new Label();
    label->setText(lab, NotificationType::dontSendNotification);
    label->setJustificationType(just);
    label->setBounds(10, ypos, 100, 22);
    label->setFont(this->grisFeel->getFont());
    label->setLookAndFeel(this->grisFeel);
    label->setColour(Label::textColourId, this->grisFeel->getFontColour());
    this->juce::Component::addAndMakeVisible(label);
    return label;
}

TextEditor *
WindowProperties::createPropIntTextEditor(String tooltip, int ypos, int init) {
    TextEditor *editor = new TextEditor();
    editor->setTooltip(tooltip);
    editor->setTextToShowWhenEmpty("", this->grisFeel->getOffColour());
    editor->setColour(ToggleButton::textColourId, this->grisFeel->getFontColour());
    editor->setLookAndFeel(this->grisFeel);
    editor->setBounds(130, ypos, 120, 22);
    editor->setInputRestrictions(5, "0123456789");
    editor->setText(String(init));
    /* Implemented but not yet in current Juce release. */
    //this->tedOSCInPort->setJustification(Justification::right);
    this->juce::Component::addAndMakeVisible(editor);
    return editor;
}

ComboBox *
WindowProperties::createPropComboBox(const StringArray choices, int selected, int ypos) {
    ComboBox *combo = new ComboBox();
    combo->addItemList(choices, 1);
    combo->setSelectedItemIndex(selected);
    combo->setBounds(130, ypos, 120, 22);
    combo->setLookAndFeel(this->grisFeel);
    this->juce::Component::addAndMakeVisible(combo);
    return combo;
}

WindowProperties::WindowProperties(const String& name, Colour backgroundColour, int buttonsNeeded,
                                     MainContentComponent * parent, GrisLookAndFeel * feel, int indR, 
                                    int indB, int indFF, int oscPort):
    DocumentWindow (name, backgroundColour, buttonsNeeded)
{
    this->mainParent = parent;
    this->grisFeel = feel;

    this->generalLabel = this->createPropLabel("General Settings", Justification::left, 20);

    this->labOSCInPort = this->createPropLabel("OSC Input Port :", Justification::left, 50);
    this->tedOSCInPort = this->createPropIntTextEditor("Port Socket OSC Input", 50, oscPort);

    this->jackSettingsLabel = this->createPropLabel("Jack Settings", Justification::left, 90);

    this->labRate = this->createPropLabel("Sampling Rate (hz) :", Justification::left, 120);
    this->cobRate = this->createPropComboBox(RateValues, indR, 120);

    this->labBuff = this->createPropLabel("Buffer Size (spls) :", Justification::left, 150);
    this->cobBuffer = this->createPropComboBox(BufferSize, indB, 150);

    this->recordingLabel = this->createPropLabel("Recording Settings", Justification::left, 190);

    this->labRecFormat = this->createPropLabel("File Format :", Justification::left, 220);
    this->recordFormat = this->createPropComboBox(FileFormats, indFF, 220);

    // -------------- Save button -------------
    this->butValidSettings = new TextButton();
    this->butValidSettings->setButtonText("Save");
    this->butValidSettings->setBounds(163, 260, 88, 22);
    this->butValidSettings->addListener(this);
    this->butValidSettings->setColour(ToggleButton::textColourId, this->grisFeel->getFontColour());
    this->butValidSettings->setLookAndFeel(this->grisFeel);
    this->juce::Component::addAndMakeVisible(this->butValidSettings);
}

WindowProperties::~WindowProperties() {
    delete this->generalLabel;
    delete this->jackSettingsLabel;
    delete this->recordingLabel;
    delete this->labOSCInPort;
    delete this->labRate;
    delete this->labBuff;
    delete this->labRecFormat;
    delete this->tedOSCInPort;
    delete this->cobRate;
    delete this->cobBuffer;
    delete this->recordFormat;
    delete this->butValidSettings;
    this->mainParent->destroyWindowProperties();
}

void WindowProperties::closeButtonPressed()
{
    delete this;
}


void WindowProperties::buttonClicked(Button *button)
{
    if( button == this->butValidSettings) {
        this->mainParent->saveProperties(this->cobRate->getText().getIntValue(),
                                           this->cobBuffer->getText().getIntValue(),
                                           this->recordFormat->getSelectedItemIndex(),
                                           this->tedOSCInPort->getTextValue().toString().getIntValue());
        delete this;
    }
}


//======================================= About Window ===========================
AboutWindow::AboutWindow(const String& name, Colour backgroundColour, int buttonsNeeded,
                              MainContentComponent * parent, GrisLookAndFeel * feel):
    DocumentWindow(name, backgroundColour, buttonsNeeded)
{
    this->mainParent = parent;
    this->grisFeel = feel;

    File fs;
#ifdef __linux__
    fs = File ( File::getCurrentWorkingDirectory().getFullPathName() + ("/../../Resources/ServerGRIS_icon_small.png"));
#else
    String cwd = File::getSpecialLocation(File::currentApplicationFile).getFullPathName();
    fs = File (cwd + ("/Contents/Resources/ServerGRIS_icon_small.png"));
#endif
    if(fs.exists()) {
        Image img = ImageFileFormat::loadFrom(fs);
        this->imageComponent = new ImageComponent("");
        this->imageComponent->setImage(img);
        this->imageComponent->setBounds(136, 5, 128, 127);
        this->juce::Component::addAndMakeVisible(this->imageComponent);
    }

    this->title = new Label("AboutBox_title");
    this->title->setText("ServerGRIS - Sound Spatialization Tool\n\n",
                         NotificationType::dontSendNotification);
    this->title->setJustificationType(Justification::horizontallyCentred);
    this->title->setBounds(5, 150, 390, 50);
    this->title->setLookAndFeel(this->grisFeel);
    this->title->setColour(Label::textColourId, this->grisFeel->getFontColour());
    this->juce::Component::addAndMakeVisible(this->title);

    this->version = new Label("AboutBox_version");
    String version_num = STRING(JUCE_APP_VERSION);
    this->version->setText("Version " + version_num + "\n\n\n",
                           NotificationType::dontSendNotification);
    this->version->setJustificationType(Justification::horizontallyCentred);
    this->version->setBounds(5, 180, 390, 50);
    this->version->setLookAndFeel(this->grisFeel);
    this->version->setColour(Label::textColourId, this->grisFeel->getFontColour());
    this->juce::Component::addAndMakeVisible(this->version);

    String infos;
    infos << "Developed by the G.R.I.S. at Université de Montréal\n\n";
    infos << "(Groupe de Recherche en Immersion Spatiale)\n\n\n";
    infos << "Director:\n\n";
    infos << "Robert NORMANDEAU\n\n\n";
    infos << "Programmers:\n\n";
    infos << "Actual: Olivier BÉLANGER\n\n";
    infos << "Former: Vincent BERTHIAUME, Nicolas MASSON, Antoine MISSOUT\n\n\n";
    infos << "Assistants:\n\n";
    infos << "David LEDOUX, Christophe LENGELÉ, Vincent MONASTESSE\n\n";

    this->label = new Label();
    this->label->setText(infos, NotificationType::dontSendNotification);
    this->label->setJustificationType(Justification::left);
    this->label->setBounds(5, 230, 390, 250);
    this->label->setFont(this->grisFeel->getFont());
    this->label->setLookAndFeel(this->grisFeel);
    this->label->setColour(Label::textColourId, this->grisFeel->getFontColour());
    this->juce::Component::addAndMakeVisible(this->label);

    // -------------- Save button -------------
    this->close = new TextButton();
    this->close->setButtonText("Close");
    this->close->setBounds(150, 470, 100, 22);
    this->close->addListener(this);
    this->close->setColour(ToggleButton::textColourId, this->grisFeel->getFontColour());
    this->close->setLookAndFeel(this->grisFeel);
    this->juce::Component::addAndMakeVisible(this->close);
}

AboutWindow::~AboutWindow() {
    delete this->imageComponent;
    delete this->title;
    delete this->version;
    delete this->label;
    delete this->close;
    this->mainParent->destroyAboutWindow();
}

void AboutWindow::closeButtonPressed()
{
    delete this;
}

void AboutWindow::buttonClicked(Button *button) {
    delete this;
}
