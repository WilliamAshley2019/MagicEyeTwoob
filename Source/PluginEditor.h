#pragma once
#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <cmath>
#include <functional>

//==============================================================================
// Interactive Magic Eye Component (with electrode zones)
//==============================================================================
class MagicEyeComponent : public juce::Component, private juce::Timer
{
public:
    MagicEyeComponent();
    ~MagicEyeComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Data from processor
    void setMeterData(float grL, float grR, float highL, float highR, float spread, float decay, float tilt);
    void setCurrentParameters(float cath, float grid, float anode);   // declaration only

    // Callback to update processor parameters when the user drags on the tube
    void setParameterCallback(std::function<void(float cathode, float grid, float anode)> callback);

    // Mouse interaction for electrode zones
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void drawMagicEye(juce::Graphics& g, float grValue, float highValue, float spread, float tilt,
        bool isLeftChannel, juce::Rectangle<float> bounds);
    void drawPhosphorGlow(juce::Graphics& g, const juce::Path& eyePath, float intensity, float highContribution);
    void drawShadowWedge(juce::Graphics& g, juce::Rectangle<float> bounds, float grValue, float highValue,
        float spread, float tilt, bool isLeftChannel);
    void drawCentralPupil(juce::Graphics& g, juce::Point<float> center, float radius, float grValue, float spread);
    void drawElectrodeZones(juce::Graphics& g, juce::Rectangle<float> bounds, bool isLeftChannel);

    // Current parameter values for drag start
    float currentCathodeTemp = 0.5f, currentGridVoltage = 0.5f, currentAnodeCurrent = 0.0f;

    // Hit testing
    enum Zone { ZoneNone, ZoneCathode, ZoneGrid, ZoneAnode };
    Zone getZoneAt(juce::Point<float> pos, juce::Rectangle<float> tubeBounds);
    void updateParameterFromDrag(Zone zone, float relativeValue, const juce::MouseEvent& e);

    // Current meter values
    float grLeft{ 0.0f }, grRight{ 0.0f };
    float highLeft{ 0.0f }, highRight{ 0.0f };
    float glowSpread{ 0.5f }, tubeDecay{ 1.0f }, spectrumTilt{ 0.0f };
    float glowPulse{ 0.0f };

    // Parameter callback
    std::function<void(float, float, float)> paramCallback;

    // Drag state
    Zone activeZone = ZoneNone;
    juce::Point<float> dragStartPos;
    float dragStartCathode = 0.5f, dragStartGrid = 0.5f, dragStartAnode = 0.0f;

    // Colours
    const juce::Colour phosphorGreen{ 0xff39ff39 };
    const juce::Colour phosphorDim{ 0xff0a550a };
    const juce::Colour glassHighlight{ 0x30ffffff };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MagicEyeComponent)
};

//==============================================================================
// Retro LookAndFeel (blackwood knobs, vintage styling)
//==============================================================================
class RetroLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RetroLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
        float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
        juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
        int buttonX, int buttonY, int buttonW, int buttonH,
        juce::ComboBox&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
    void drawLabel(juce::Graphics&, juce::Label&) override;

private:
    juce::Font retroFont;
};

//==============================================================================
// Main Editor
//==============================================================================
class TL64MagicEyeEditor : public juce::AudioProcessorEditor,
    private juce::AudioProcessorValueTreeState::Listener,
    private juce::Timer
{
public:
    TL64MagicEyeEditor(TL64MagicEyeProcessor&);
    ~TL64MagicEyeEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void timerCallback() override;

    // Called from MagicEyeComponent when user drags on tube electrodes
    void updateTubeParameters(float cathodeTemp, float gridVoltage, float anodeCurrent);

private:
    MagicEyeComponent magicEye;
    RetroLookAndFeel retroLookAndFeel;

    // Existing controls
    juce::Slider driveSlider, outputSlider, responseFreqSlider, bassCompSlider, clippingSlider;
    juce::ToggleButton dynamicResponseToggle;
    juce::ComboBox oversampleCombo;

    // Labels
    juce::Label driveLabel, outputLabel, responseLabel, bassLabel, clipLabel, oversampleLabel;

    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        driveAtt, outputAtt, responseFreqAtt, bassCompAtt, clippingAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> dynamicAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversampleAtt;

    TL64MagicEyeProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TL64MagicEyeEditor)
};