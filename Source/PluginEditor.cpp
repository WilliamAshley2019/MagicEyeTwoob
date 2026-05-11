#include "PluginEditor.h"

//==============================================================================
// MagicEyeComponent (Interactive)
//==============================================================================
MagicEyeComponent::MagicEyeComponent()
{
    startTimerHz(30);
}

void MagicEyeComponent::timerCallback()
{
    static float phase = 0.0f;
    phase += 0.08f;
    glowPulse = 0.8f + 0.2f * std::sin(phase);
    repaint();
}

void MagicEyeComponent::setMeterData(float grL, float grR, float highL, float highR,
    float spread, float decay, float tilt)
{
    grLeft = juce::jlimit(0.0f, 1.0f, grL);
    grRight = juce::jlimit(0.0f, 1.0f, grR);
    highLeft = juce::jlimit(0.0f, 1.0f, highL);
    highRight = juce::jlimit(0.0f, 1.0f, highR);
    glowSpread = juce::jlimit(0.0f, 1.0f, spread);
    tubeDecay = decay;
    spectrumTilt = tilt;
}

void MagicEyeComponent::setCurrentParameters(float cath, float grid, float anode)
{
    currentCathodeTemp = cath;
    currentGridVoltage = grid;
    currentAnodeCurrent = anode;
}

void MagicEyeComponent::setParameterCallback(std::function<void(float, float, float)> callback)
{
    paramCallback = callback;
}

//==============================================================================
// Zone hit testing
MagicEyeComponent::Zone MagicEyeComponent::getZoneAt(juce::Point<float> pos, juce::Rectangle<float> tubeBounds)
{
    float y = (pos.y - tubeBounds.getY()) / tubeBounds.getHeight();
    if (y < 0.25f) return ZoneAnode;
    if (y > 0.75f) return ZoneCathode;
    return ZoneGrid;
}

void MagicEyeComponent::updateParameterFromDrag(Zone zone, float relativeValue, const juce::MouseEvent&)
{
    if (!paramCallback) return;
    switch (zone)
    {
    case ZoneCathode:
        paramCallback(relativeValue, dragStartGrid, dragStartAnode);
        dragStartCathode = relativeValue;
        break;
    case ZoneGrid:
        paramCallback(dragStartCathode, relativeValue, dragStartAnode);
        dragStartGrid = relativeValue;
        break;
    case ZoneAnode:
    {
        float anodeDb = -12.0f + relativeValue * 24.0f;
        paramCallback(dragStartCathode, dragStartGrid, anodeDb);
        dragStartAnode = anodeDb;
    }
    break;
    default: break;
    }
}

void MagicEyeComponent::mouseDown(const juce::MouseEvent& e)
{
    auto bounds = getLocalBounds().toFloat();
    auto leftTube = bounds.withWidth(bounds.getWidth() * 0.42f)
        .withX(bounds.getX() + bounds.getWidth() * 0.06f);
    auto rightTube = bounds.withWidth(bounds.getWidth() * 0.42f)
        .withX(bounds.getRight() - bounds.getWidth() * 0.48f);

    if (leftTube.contains(e.position))
        activeZone = getZoneAt(e.position, leftTube);
    else if (rightTube.contains(e.position))
        activeZone = getZoneAt(e.position, rightTube);
    else
        activeZone = ZoneNone;

    if (activeZone != ZoneNone)
    {
        dragStartPos = e.position;
        dragStartCathode = currentCathodeTemp;
        dragStartGrid = currentGridVoltage;
        dragStartAnode = currentAnodeCurrent;
    }
}

void MagicEyeComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (activeZone == ZoneNone) return;
    auto bounds = getLocalBounds().toFloat();
    auto leftTube = bounds.withWidth(bounds.getWidth() * 0.42f)
        .withX(bounds.getX() + bounds.getWidth() * 0.06f);
    auto rightTube = bounds.withWidth(bounds.getWidth() * 0.42f)
        .withX(bounds.getRight() - bounds.getWidth() * 0.48f);
    juce::Rectangle<float> tubeBounds = leftTube;
    if (rightTube.contains(e.position)) tubeBounds = rightTube;

    float y = (e.position.y - tubeBounds.getY()) / tubeBounds.getHeight();
    float x = (e.position.x - tubeBounds.getX()) / tubeBounds.getWidth();
    y = juce::jlimit(0.0f, 1.0f, y);
    x = juce::jlimit(0.0f, 1.0f, x);

    float relativeValue = 0.0f;
    if (activeZone == ZoneCathode)
        relativeValue = 1.0f - y;
    else if (activeZone == ZoneGrid)
        relativeValue = x;
    else if (activeZone == ZoneAnode)
    {
        auto centre = tubeBounds.getCentre();
        float angle = std::atan2(e.position.y - centre.y, e.position.x - centre.x);
        relativeValue = (angle + juce::MathConstants<float>::pi) / (2.0f * juce::MathConstants<float>::pi);
        relativeValue = juce::jlimit(0.0f, 1.0f, relativeValue);
    }
    updateParameterFromDrag(activeZone, relativeValue, e);
    repaint();
}

void MagicEyeComponent::mouseUp(const juce::MouseEvent&)
{
    activeZone = ZoneNone;
}
//==============================================================================
// Drawing routines
void MagicEyeComponent::drawShadowWedge(juce::Graphics& g, juce::Rectangle<float> bounds,
    float grValue, float /*highValue*/,
    float spread, float tilt, bool isLeftChannel)
{
    auto center = bounds.getCentre();
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.45f;
    const float degToRad = juce::MathConstants<float>::pi / 180.0f;

    // Map GR to shadow angle (more GR = smaller angle = more closed)
    float baseAngle = juce::jmap(grValue, 80.0f, 10.0f);
    // Apply spread + tilt
    float asymmetry = spread * 40.0f + tilt * 20.0f;
    if (!isLeftChannel) asymmetry = -asymmetry;

    float leftAngle = juce::jlimit(5.0f, 85.0f, baseAngle + asymmetry);
    float rightAngle = juce::jlimit(5.0f, 85.0f, baseAngle - asymmetry);

    // Left wedge
    juce::Path leftWedge;
    float leftStart = (90.0f + leftAngle) * degToRad;
    float leftEnd = (270.0f - leftAngle) * degToRad;
    leftWedge.startNewSubPath(center);
    leftWedge.addArc(bounds.getX(), bounds.getY(), radius * 2.0f, radius * 2.0f, leftStart, leftEnd, true);
    leftWedge.closeSubPath();

    // Right wedge
    juce::Path rightWedge;
    float rightStart = (90.0f - rightAngle) * degToRad;
    float rightEnd = (270.0f + rightAngle) * degToRad;
    rightWedge.startNewSubPath(center);
    rightWedge.addArc(bounds.getX(), bounds.getY(), radius * 2.0f, radius * 2.0f, rightStart, rightEnd, true);
    rightWedge.closeSubPath();

    juce::ColourGradient shadowGrad(
        juce::Colours::black.withAlpha(0.9f), center.x, center.y,
        juce::Colours::transparentBlack, center.x + radius * 0.5f, center.y + radius, true);
    g.setGradientFill(shadowGrad);
    g.fillPath(leftWedge);
    g.fillPath(rightWedge);
}

void MagicEyeComponent::drawCentralPupil(juce::Graphics& g, juce::Point<float> center,
    float radius, float grValue, float spread)
{
    float baseRadius = radius * (0.25f + 0.15f * (1.0f - grValue));
    float horizRadius = baseRadius * (1.0f + spread * 0.5f);
    float vertRadius = baseRadius * 0.7f;
    g.setColour(juce::Colours::black);
    g.fillEllipse(center.x - horizRadius, center.y - vertRadius,
        horizRadius * 2.0f, vertRadius * 2.0f);
    // Glass highlight
    g.setColour(juce::Colours::white.withAlpha(0.4f * (1.0f - grValue)));
    g.fillEllipse(center.x - horizRadius * 0.4f, center.y - vertRadius * 0.4f,
        horizRadius * 0.6f, vertRadius * 0.6f);
}

void MagicEyeComponent::drawPhosphorGlow(juce::Graphics& g, const juce::Path& eyePath,
    float intensity, float highContribution)
{
    auto bounds = eyePath.getBounds();
    auto center = bounds.getCentre();
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.4f;

    // Colour shifts from green to white/cyan as high frequencies increase
    juce::Colour glowColor = phosphorGreen.interpolatedWith(juce::Colours::white, highContribution * 0.8f);
    glowColor = glowColor.withMultipliedAlpha(0.9f * intensity * glowPulse);

    juce::ColourGradient glowGrad(
        glowColor, center.x, center.y,
        phosphorDim.withAlpha(0.2f), center.x + radius * 0.8f, center.y + radius * 0.8f, true);
    g.setGradientFill(glowGrad);
    g.fillPath(eyePath);

    if (intensity > 0.1f)
    {
        juce::DropShadow bloom(glowColor.withAlpha(0.4f * intensity), 12, juce::Point<int>(0, 0));
        bloom.drawForPath(g, eyePath);
    }
}

void MagicEyeComponent::drawElectrodeZones(juce::Graphics& g, juce::Rectangle<float> bounds, bool isLeftChannel)
{
    auto area = bounds.reduced(10);
    float cathodeY = area.getBottom() - area.getHeight() * 0.2f;
    float gridY = area.getCentreY();
    float anodeY = area.getY() + area.getHeight() * 0.2f;

    // Draw translucent overlays to indicate draggable zones (only visible when hovering? for clarity we draw faintly)
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    // Cathode zone (bottom)
    g.fillRect(area.getX(), cathodeY, area.getWidth(), area.getHeight() * 0.2f);
    // Grid zone (middle)
    g.fillRect(area.getX(), gridY - area.getHeight() * 0.1f, area.getWidth(), area.getHeight() * 0.2f);
    // Anode zone (top)
    g.fillRect(area.getX(), anodeY, area.getWidth(), area.getHeight() * 0.2f);

    // Labels
    g.setColour(juce::Colours::beige.withAlpha(0.7f));
    juce::Font smallFont(juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain);
    g.setFont(smallFont);
    g.drawText("CATHODE", area.getX(), cathodeY + 2, area.getWidth(), 12, juce::Justification::centred);
    g.drawText("GRID", area.getX(), gridY - 8, area.getWidth(), 12, juce::Justification::centred);
    g.drawText("ANODE", area.getX(), anodeY + 2, area.getWidth(), 12, juce::Justification::centred);
}

void MagicEyeComponent::drawMagicEye(juce::Graphics& g, float grValue, float highValue,
    float spread, float tilt, bool isLeftChannel,
    juce::Rectangle<float> bounds)
{
    auto center = bounds.getCentre();
    float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.45f;

    // Glass envelope
    juce::Path glassEnvelope;
    glassEnvelope.addEllipse(bounds.reduced(4).toFloat());
    juce::ColourGradient glassGrad(
        juce::Colours::white.withAlpha(0.15f), center.x, center.y - radius,
        juce::Colours::transparentWhite, center.x, center.y + radius, false);
    g.setGradientFill(glassGrad);
    g.fillPath(glassEnvelope);
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.strokePath(glassEnvelope, juce::PathStrokeType(1.5f));

    // Dark phosphor background
    juce::Path phosphorBg;
    phosphorBg.addEllipse(bounds.reduced(8).toFloat());
    g.setColour(phosphorDim);
    g.fillPath(phosphorBg);

    // Shadow wedges
    drawShadowWedge(g, bounds.reduced(10), grValue, highValue, spread, tilt, isLeftChannel);

    // Active phosphor (neon glow)
    juce::Path activePhosphor;
    activePhosphor.addEllipse(bounds.reduced(10).toFloat());
    float intensity = juce::jmap(grValue, 0.2f, 0.95f);
    drawPhosphorGlow(g, activePhosphor, intensity, highValue);

    // Grid lines (cathode structure)
    g.setColour(juce::Colours::black.withAlpha(0.3f));
    g.drawHorizontalLine(center.getY(), center.x - radius * 0.7f, center.x + radius * 0.7f);
    g.drawVerticalLine(center.getX(), center.y - radius * 0.6f, center.y + radius * 0.6f);

    // Central pupil
    drawCentralPupil(g, center, radius * 0.6f, grValue, spread);

    // Interactive electrode zones overlay
    drawElectrodeZones(g, bounds, isLeftChannel);
}

void MagicEyeComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    // Dark vacuum tube background
    juce::ColourGradient bgGrad(
        juce::Colour(0xff0a0a10), 0, 0,
        juce::Colour(0xff050508), 0, bounds.getHeight(), false);
    g.setGradientFill(bgGrad);
    g.fillRect(bounds);

    // Left Magic Eye
    auto eyeBounds = bounds.withWidth(bounds.getWidth() * 0.42f)
        .withX(bounds.getX() + bounds.getWidth() * 0.06f)
        .withTrimmedBottom(0);
    drawMagicEye(g, grLeft, highLeft, glowSpread, spectrumTilt, true, eyeBounds);

    // Right Magic Eye
    eyeBounds = bounds.withWidth(bounds.getWidth() * 0.42f)
        .withX(bounds.getRight() - bounds.getWidth() * 0.48f)
        .withTrimmedBottom(0);
    drawMagicEye(g, grRight, highRight, glowSpread, spectrumTilt, false, eyeBounds);

    // Title
    g.setColour(juce::Colours::white.withAlpha(0.7f));
    juce::Font titleFont(juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::bold);
    g.setFont(titleFont);
    g.drawFittedText("TL-64 MAGIC EYE", bounds.removeFromTop(30.0f).toNearestInt(),
        juce::Justification::centred, 1);
}

void MagicEyeComponent::resized() {}

//==============================================================================
// RetroLookAndFeel Implementation
//==============================================================================
RetroLookAndFeel::RetroLookAndFeel()
{
    retroFont = juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain);
    setDefaultSansSerifTypeface(juce::Typeface::createSystemTypefaceFor(retroFont));
}

void RetroLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
    juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>(x, y, width, height);
    auto radius = juce::jmin(bounds.getWidth() / 2.0f, bounds.getHeight() / 2.0f) - 6.0f;
    auto centre = bounds.getCentre();

    // Blackwood base (darkest ebony)
    juce::Colour blackwood(0xff1a0f0a);
    juce::Colour blackwoodHighlight(0xff2c1e15);
    g.setColour(blackwood);
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(blackwoodHighlight);
    g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 2.0f);

    // Grain lines
    for (int i = 0; i < 8; ++i)
    {
        float angle = i * juce::MathConstants<float>::twoPi / 8.0f;
        float r1 = radius * 0.6f, r2 = radius * 0.9f;
        float x1 = centre.x + r1 * std::cos(angle);
        float y1 = centre.y + r1 * std::sin(angle);
        float x2 = centre.x + r2 * std::cos(angle);
        float y2 = centre.y + r2 * std::sin(angle);
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.drawLine(x1, y1, x2, y2, 1.0f);
    }

    // Pointer
    float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    juce::Path pointer;
    pointer.addRectangle(-2.0f, -radius + 8.0f, 4.0f, radius * 0.35f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centre));
    g.setColour(juce::Colours::beige);
    g.fillPath(pointer);

    g.setFont(retroFont);
    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.drawText(slider.getTextFromValue(slider.getValue()), bounds, juce::Justification::centredTop, false);
}

void RetroLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
    bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat();
    auto toggleW = bounds.getHeight() * 1.8f;
    auto toggleRect = bounds.withWidth(toggleW).withX(bounds.getX() + 2.0f);
    bool isOn = button.getToggleState();

    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(toggleRect, 4.0f);
    auto leverRect = toggleRect.withWidth(toggleRect.getWidth() / 2.0f)
        .withX(isOn ? toggleRect.getRight() - toggleRect.getWidth() / 2.0f : toggleRect.getX());
    g.setColour(juce::Colour(0xffbc9a6c));
    g.fillRoundedRectangle(leverRect, 3.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(toggleRect, 4.0f, 1.0f);

    g.setColour(juce::Colours::beige);
    g.setFont(retroFont);
    g.drawText(button.getButtonText(), bounds, juce::Justification::centredLeft);
}

void RetroLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
    int buttonX, int buttonY, int buttonW, int buttonH,
    juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(juce::Colour(0xffbc9a6c));
    g.drawRoundedRectangle(bounds, 4.0f, 1.5f);

    juce::String text = box.getText();
    g.setColour(juce::Colours::white);
    g.setFont(retroFont);
    g.drawText(text, bounds.reduced(4), juce::Justification::centredLeft);

    juce::Path arrow;
    arrow.addTriangle(buttonX + buttonW * 0.5f, buttonY + buttonH * 0.3f,
        buttonX + buttonW * 0.2f, buttonY + buttonH * 0.7f,
        buttonX + buttonW * 0.8f, buttonY + buttonH * 0.7f);
    g.setColour(juce::Colours::beige);
    g.fillPath(arrow);
}

juce::Font RetroLookAndFeel::getTextButtonFont(juce::TextButton&, int /*buttonHeight*/)
{
    return retroFont;
}

void RetroLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.setColour(juce::Colours::beige.withAlpha(0.8f));
    g.setFont(retroFont);
    g.drawText(label.getText(), label.getLocalBounds(), juce::Justification::centred, false);
}

//==============================================================================
// TL64MagicEyeEditor
//==============================================================================
TL64MagicEyeEditor::TL64MagicEyeEditor(TL64MagicEyeProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(820, 540);
    setLookAndFeel(&retroLookAndFeel);
    addAndMakeVisible(magicEye);

    auto setupRotary = [this](juce::Slider& s, const juce::String& labelText, juce::Label& label)
        {
            s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            addAndMakeVisible(s);
            label.setText(labelText, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centred);
            addAndMakeVisible(label);
        };

    setupRotary(driveSlider, "DRIVE", driveLabel);
    driveSlider.setRange(0.0, 24.0, 0.1);
    setupRotary(outputSlider, "OUTPUT", outputLabel);
    outputSlider.setRange(-24.0, 24.0, 0.1);
    setupRotary(bassCompSlider, "BASS COMP", bassLabel);
    bassCompSlider.setRange(-12.0, 12.0, 0.1);

    responseFreqSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    responseFreqSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 60, 20);
    addAndMakeVisible(responseFreqSlider);
    responseLabel.setText("RESPONSE", juce::dontSendNotification);
    addAndMakeVisible(responseLabel);

    clippingSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    clippingSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 60, 20);
    clippingSlider.setRange(0.0, 1.0, 0.01);
    addAndMakeVisible(clippingSlider);
    clipLabel.setText("CLIPPING", juce::dontSendNotification);
    addAndMakeVisible(clipLabel);

    dynamicResponseToggle.setButtonText("DYNAMIC");
    addAndMakeVisible(dynamicResponseToggle);

    oversampleCombo.addItem("1x", 1); oversampleCombo.addItem("2x", 2);
    oversampleCombo.addItem("4x", 3); oversampleCombo.addItem("8x", 4);
    oversampleCombo.setSelectedId(2);
    addAndMakeVisible(oversampleCombo);
    oversampleLabel.setText("OVERSAMPLE", juce::dontSendNotification);
    addAndMakeVisible(oversampleLabel);

    auto& apvts = processor.getAPVTS();
    driveAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "drive", driveSlider);
    outputAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "outputGain", outputSlider);
    responseFreqAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "responseFreq", responseFreqSlider);
    bassCompAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "bassComp", bassCompSlider);
    clippingAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "clipping", clippingSlider);
    dynamicAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "dynamicResponse", dynamicResponseToggle);
    oversampleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "oversampling", oversampleCombo);

    magicEye.setParameterCallback([this](float cathode, float grid, float anode)
        {
            updateTubeParameters(cathode, grid, anode);
        });

    startTimerHz(30);
}

TL64MagicEyeEditor::~TL64MagicEyeEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void TL64MagicEyeEditor::updateTubeParameters(float cathodeTemp, float gridVoltage, float anodeCurrent)
{
    auto& apvts = processor.getAPVTS();
    if (auto* param = apvts.getParameter("cathodeTemp"))
        param->setValueNotifyingHost(param->convertTo0to1(cathodeTemp));
    if (auto* param = apvts.getParameter("gridVoltage"))
        param->setValueNotifyingHost(param->convertTo0to1(gridVoltage));
    if (auto* param = apvts.getParameter("anodeCurrent"))
        param->setValueNotifyingHost(param->convertTo0to1(anodeCurrent));
}

void TL64MagicEyeEditor::timerCallback()
{
    // Fetch current parameters from processor
    float grL = processor.getLeftGRMeter();
    float grR = processor.getRightGRMeter();

    // Get smoothed high‑frequency energy and the fast peak
    float smoothL = processor.getHighFreqEnergyLeft();
    float smoothR = processor.getHighFreqEnergyRight();
    float peakL = processor.getHighFreqPeakLeft();
    float peakR = processor.getHighFreqPeakRight();

    // Blend: 70% smooth + 30% peak for extra transient sparkle
    float highL = smoothL * 0.7f + peakL * 0.3f;
    float highR = smoothR * 0.7f + peakR * 0.3f;

    float spread = processor.getAPVTS().getRawParameterValue("panBias")->load();
    float decay = processor.getAPVTS().getRawParameterValue("tubeDecay")->load();
    float tilt = processor.getAPVTS().getRawParameterValue("spectrumTilt")->load();

    magicEye.setMeterData(grL, grR, highL, highR, spread, decay, tilt);
    magicEye.setCurrentParameters(processor.getCurrentCathodeTemp(),
        processor.getCurrentGridVoltage(),
        processor.getCurrentAnodeCurrent());
}

void TL64MagicEyeEditor::parameterChanged(const juce::String&, float)
{
    repaint();
}

void TL64MagicEyeEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a0a12));
}

void TL64MagicEyeEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    auto topArea = area.removeFromTop(static_cast<int>(area.getHeight() * 0.55f));
    magicEye.setBounds(topArea);

    auto bottomArea = area;
    auto row1 = bottomArea.removeFromTop(bottomArea.getHeight() / 2).reduced(8);
    auto row2 = bottomArea.reduced(8);

    int knobWidth = row1.getWidth() / 3;
    driveSlider.setBounds(row1.removeFromLeft(knobWidth).reduced(15));
    outputSlider.setBounds(row1.removeFromLeft(knobWidth).reduced(15));
    bassCompSlider.setBounds(row1.reduced(15));

    auto row2Left = row2.removeFromLeft(row2.getWidth() / 2);
    responseFreqSlider.setBounds(row2Left.removeFromLeft(row2Left.getWidth() / 2).reduced(10));
    clippingSlider.setBounds(row2Left.reduced(10));
    auto row2Right = row2;
    oversampleCombo.setBounds(row2Right.removeFromLeft(row2Right.getWidth() / 2).reduced(10));
    dynamicResponseToggle.setBounds(row2Right.reduced(10));

    auto placeLabel = [](juce::Slider& s, juce::Label& l) {
        auto b = s.getBounds();
        l.setBounds(b.getX(), b.getBottom() - 18, b.getWidth(), 16);
        };
    placeLabel(driveSlider, driveLabel);
    placeLabel(outputSlider, outputLabel);
    placeLabel(bassCompSlider, bassLabel);
    responseLabel.setBounds(responseFreqSlider.getBounds().getX(),
        responseFreqSlider.getBounds().getBottom() - 18,
        responseFreqSlider.getWidth(), 16);
    clipLabel.setBounds(clippingSlider.getBounds().getX(),
        clippingSlider.getBounds().getBottom() - 18,
        clippingSlider.getWidth(), 16);
    oversampleLabel.setBounds(oversampleCombo.getBounds().getX(),
        oversampleCombo.getBounds().getBottom() - 18,
        oversampleCombo.getWidth(), 16);
}