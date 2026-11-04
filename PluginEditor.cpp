#include "PluginEditor.h"

AlphaFastDistEditor::AlphaFastDistEditor(AlphaFastDistProcessor& p)
    : AudioProcessorEditor(&p), proc(p)
{
    setSize(560, 420);
    setResizable(false, false);

    auto setupKnob = [this](juce::Slider& k, const juce::String& id,
        std::unique_ptr<Attachment>& att)
        {
            k.setLookAndFeel(&lnf);
            k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            k.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            addAndMakeVisible(k);
            att = std::make_unique<Attachment>(proc.apvts, id, k);
        };

    setupKnob(preKnob,       "pre",       preAtt);
    setupKnob(thresholdKnob, "threshold", thresholdAtt);
    setupKnob(mixKnob,       "mix",       mixAtt);
    setupKnob(postKnob,      "post",      postAtt);

    titleLabel.setText("ALPHA FAST DIST", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(22.0f).withStyle("Bold")));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(220, 220, 220));
    addAndMakeVisible(titleLabel);

    auto knobLabel = [this](juce::Label& l, const juce::String& t) {
        l.setText(t, juce::dontSendNotification);
        l.setFont(juce::Font(juce::FontOptions(11.0f)));
        l.setJustificationType(juce::Justification::centred);
        l.setColour(juce::Label::textColourId, juce::Colour(180, 180, 185));
        addAndMakeVisible(l);
        };
    knobLabel(preLabel,       "PRE");
    knobLabel(thresholdLabel, "THRESHOLD");
    knobLabel(mixLabel,       "MIX");
    knobLabel(postLabel,      "POST");

    typeLabel.setText("TYPE", juce::dontSendNotification);
    typeLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    typeLabel.setJustificationType(juce::Justification::centred);
    typeLabel.setColour(juce::Label::textColourId, juce::Colour(180, 180, 185));
    addAndMakeVisible(typeLabel);

    typeButton.onClick = [this] { onTypeClicked(); };
    addAndMakeVisible(typeButton);

    presetLcd.setLabel("PRESET");
    addAndMakeVisible(presetLcd);

    preLcd.setLabel("PRE");
    addAndMakeVisible(preLcd);
    thresholdLcd.setLabel("THRESH");
    addAndMakeVisible(thresholdLcd);
    mixLcd.setLabel("MIX");
    addAndMakeVisible(mixLcd);
    postLcd.setLabel("POST");
    addAndMakeVisible(postLcd);

    preLcd.onMouseDown = [this](const juce::MouseEvent&) {
        if (activeEdit == 0) showEditorFor(1);
        };
    thresholdLcd.onMouseDown = [this](const juce::MouseEvent&) {
        if (activeEdit == 0) showEditorFor(2);
        };
    mixLcd.onMouseDown = [this](const juce::MouseEvent&) {
        if (activeEdit == 0) showEditorFor(3);
        };
    postLcd.onMouseDown = [this](const juce::MouseEvent&) {
        if (activeEdit == 0) showEditorFor(4);
        };

    prevButton.onClick = [this] { changePreset(-1); };
    addAndMakeVisible(prevButton);
    nextButton.onClick = [this] { changePreset(1); };
    addAndMakeVisible(nextButton);
    saveButton.onClick = [this] { onSaveClicked(); };
    addAndMakeVisible(saveButton);

    auto setupEditor = [this](juce::TextEditor& e) {
        e.setMultiLine(false);
        e.setReturnKeyStartsNewLine(false);
        e.setJustification(juce::Justification::centred);
        e.setFont(juce::Font(juce::FontOptions(15.0f)));
        e.setColour(juce::TextEditor::backgroundColourId,      juce::Colour(120, 140, 100));
        e.setColour(juce::TextEditor::textColourId,            juce::Colour(20, 25, 15));
        e.setColour(juce::TextEditor::outlineColourId,         juce::Colours::transparentBlack);
        e.setColour(juce::TextEditor::focusedOutlineColourId,  juce::Colours::transparentBlack);
        e.setColour(juce::TextEditor::highlightColourId,       juce::Colour(60, 70, 50).withAlpha(0.4f));
        e.setColour(juce::CaretComponent::caretColourId,       juce::Colour(20, 25, 15));
        e.setVisible(false);
        addChildComponent(e);
        };

    setupEditor(preEdit);
    setupEditor(thresholdEdit);
    setupEditor(mixEdit);
    setupEditor(postEdit);

    preEdit.onReturnKey       = [this] { commitEdit(1); };
    thresholdEdit.onReturnKey = [this] { commitEdit(2); };
    mixEdit.onReturnKey       = [this] { commitEdit(3); };
    postEdit.onReturnKey      = [this] { commitEdit(4); };
    preEdit.onEscapeKey       = [this] { hideEditors(); };
    thresholdEdit.onEscapeKey = [this] { hideEditors(); };
    mixEdit.onEscapeKey       = [this] { hideEditors(); };
    postEdit.onEscapeKey      = [this] { hideEditors(); };
    preEdit.onFocusLost       = [this] { if (activeEdit == 1) commitEdit(1); };
    thresholdEdit.onFocusLost = [this] { if (activeEdit == 2) commitEdit(2); };
    mixEdit.onFocusLost       = [this] { if (activeEdit == 3) commitEdit(3); };
    postEdit.onFocusLost      = [this] { if (activeEdit == 4) commitEdit(4); };

    startTimerHz(30);
    updateReadouts();
}

AlphaFastDistEditor::~AlphaFastDistEditor()
{
    stopTimer();
    preAtt.reset();
    thresholdAtt.reset();
    mixAtt.reset();
    postAtt.reset();
    preKnob.setLookAndFeel(nullptr);
    thresholdKnob.setLookAndFeel(nullptr);
    mixKnob.setLookAndFeel(nullptr);
    postKnob.setLookAndFeel(nullptr);
}

void AlphaFastDistEditor::changePreset(int direction)
{
    const int current = proc.getCurrentProgram();
    const int numPrograms = proc.getNumPrograms();
    if (numPrograms == 0) return;
    int next = current + direction;
    if (next < 0) next = numPrograms - 1;
    if (next >= numPrograms) next = 0;
    proc.setCurrentProgram(next);
}

void AlphaFastDistEditor::onSaveClicked()
{
    chooser = std::make_unique<juce::FileChooser>(
        "Save preset as...",
        proc.presets.presetFolder().getChildFile("My Preset.alph"),
        "*.alph");

    chooser->launchAsync(juce::FileBrowserComponent::saveMode
                       | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File{}) return;
            if (f.getFileExtension().isEmpty())
                f = f.withFileExtension(".alph");
            juce::String err;
            if (! proc.saveCurrentAsUserPreset(f.getFileNameWithoutExtension(), err))
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Save Preset Failed", err);
                return;
            }
            timerCallback();
        });
}

void AlphaFastDistEditor::onTypeClicked()
{
    if (proc.typeParam == nullptr) return;
    const int cur = proc.typeParam->get();
    const int nxt = (cur == AlphaFastDistEffect::kTypeA)
                    ? AlphaFastDistEffect::kTypeB
                    : AlphaFastDistEffect::kTypeA;
    proc.typeParam->setValueNotifyingHost(
        proc.typeParam->convertTo0to1(static_cast<float>(nxt)));
}

void AlphaFastDistEditor::timerCallback()
{
    updateReadouts();
}

void AlphaFastDistEditor::updateReadouts()
{
    const int current = proc.getCurrentProgram();
    const auto names = proc.presets.allProgramNames();
    if (current >= 0 && current < names.size())
    {
        const juce::String t = names[current];
        if (t != lastPresetText) { presetLcd.setValue(t); lastPresetText = t; }
    }

    typeButton.setText(
        proc.typeParam != nullptr && proc.typeParam->get() == AlphaFastDistEffect::kTypeB
        ? "B" : "A");

    if (activeEdit != 0) return;

    {
        const int v = static_cast<int>(preKnob.getValue());
        const juce::String t = juce::String(v / 128.0, 2) + "x";
        if (t != lastPreText) { preLcd.setValue(t); lastPreText = t; }
        preLcd.setBar(juce::jlimit(0.0f, 1.0f, (v - 64) / 128.0f));
    }
    {
        const int v = static_cast<int>(thresholdKnob.getValue());
        const juce::String t = juce::String(v);
        if (t != lastThresholdText) { thresholdLcd.setValue(t); lastThresholdText = t; }
        thresholdLcd.setBar(juce::jlimit(0.0f, 1.0f, (v - 1) / 9.0f));
    }
    {
        const int v = static_cast<int>(mixKnob.getValue());
        const juce::String t = juce::String(juce::roundToInt(v / 1.28)) + "%";
        if (t != lastMixText) { mixLcd.setValue(t); lastMixText = t; }
        mixLcd.setBar(juce::jlimit(0.0f, 1.0f, v / 128.0f));
    }
    {
        const int v = static_cast<int>(postKnob.getValue());
        const juce::String t = juce::String(v / 128.0, 2) + "x";
        if (t != lastPostText) { postLcd.setValue(t); lastPostText = t; }
        postLcd.setBar(juce::jlimit(0.0f, 1.0f, v / 128.0f));
    }
}

void AlphaFastDistEditor::showEditorFor(int which)
{
    hideEditors();

    juce::Rectangle<int> editBounds;
    juce::TextEditor* te = nullptr;
    int value = 0;

    if (which == 1)
    {
        editBounds = preLcd.getBounds().withTrimmedTop(16).withTrimmedBottom(10);
        te = &preEdit; value = static_cast<int>(preKnob.getValue());
    }
    else if (which == 2)
    {
        editBounds = thresholdLcd.getBounds().withTrimmedTop(16).withTrimmedBottom(10);
        te = &thresholdEdit; value = static_cast<int>(thresholdKnob.getValue());
    }
    else if (which == 3)
    {
        editBounds = mixLcd.getBounds().withTrimmedTop(16).withTrimmedBottom(10);
        te = &mixEdit; value = static_cast<int>(mixKnob.getValue());
    }
    else if (which == 4)
    {
        editBounds = postLcd.getBounds().withTrimmedTop(16).withTrimmedBottom(10);
        te = &postEdit; value = static_cast<int>(postKnob.getValue());
    }

    if (te != nullptr)
    {
        activeEdit = which;
        te->setText(juce::String(value), juce::dontSendNotification);
        te->setBounds(editBounds);
        te->setVisible(true);
        te->grabKeyboardFocus();
        te->selectAll();
    }
}

void AlphaFastDistEditor::commitEdit(int which)
{
    if (activeEdit != which) return;

    juce::TextEditor* te = nullptr;
    juce::AudioParameterInt* param = nullptr;
    int lo = 0, hi = 0;

    if (which == 1)
    { te = &preEdit; param = proc.preParam; lo = 64; hi = 192; }
    else if (which == 2)
    { te = &thresholdEdit; param = proc.thresholdParam; lo = 1; hi = 10; }
    else if (which == 3)
    { te = &mixEdit; param = proc.mixParam; lo = 0; hi = 128; }
    else if (which == 4)
    { te = &postEdit; param = proc.postParam; lo = 0; hi = 128; }

    if (te != nullptr && param != nullptr)
    {
        const int value = juce::jlimit(lo, hi, te->getText().getIntValue());
        param->setValueNotifyingHost(
            param->convertTo0to1(static_cast<float>(value)));
    }

    hideEditors();
}

void AlphaFastDistEditor::hideEditors()
{
    preEdit.setVisible(false);
    thresholdEdit.setVisible(false);
    mixEdit.setVisible(false);
    postEdit.setVisible(false);
    activeEdit = 0;
    grabKeyboardFocus();
}

void AlphaFastDistEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient grad(
        juce::Colour(20, 20, 25), 0.0f, 0.0f,
        juce::Colour(10, 10, 15), 0.0f, (float)getHeight(), false);
    g.setGradientFill(grad);
    g.fillAll();

    g.setColour(juce::Colour(40, 40, 45));
    for (int x = 0; x < getWidth(); x += 20)
        g.drawLine((float)x, 0.0f, (float)x, (float)getHeight(), 0.5f);
    for (int y = 0; y < getHeight(); y += 20)
        g.drawLine(0.0f, (float)y, (float)getWidth(), (float)y, 0.5f);

    g.setColour(juce::Colour(50, 50, 55));
    g.drawRect(getLocalBounds(), 2);

    const auto footer = getLocalBounds().removeFromBottom(24);
    g.setColour(juce::Colour(100, 100, 105));
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.drawFittedText("Alpha Audio by William Ashley",
        footer, juce::Justification::centred, 1, 0.9f);
}

void AlphaFastDistEditor::resized()
{
    auto area = getLocalBounds().reduced(12);
    area.removeFromBottom(24);

    titleLabel.setBounds(area.removeFromTop(28));
    area.removeFromTop(4);

    auto presetRow = area.removeFromTop(34);
    const int btnSize = 34;
    const int gap = 6;

    prevButton.setBounds(presetRow.removeFromLeft(btnSize));
    presetRow.removeFromLeft(gap);

    const int presetLcdWidth = presetRow.getWidth() - (2 * btnSize + 2 * gap);
    presetLcd.setBounds(presetRow.removeFromLeft(presetLcdWidth));

    presetRow.removeFromLeft(gap);
    nextButton.setBounds(presetRow.removeFromLeft(btnSize));
    presetRow.removeFromLeft(gap);
    saveButton.setBounds(presetRow.removeFromLeft(btnSize));

    area.removeFromTop(12);

    juce::Slider* knobs[4]{ &preKnob, &thresholdKnob, &mixKnob, &postKnob };
    juce::Label*  labels[4]{ &preLabel, &thresholdLabel, &mixLabel, &postLabel };

    const int knobSize = 96;
    const int labelH = 16;
    const int colGap = 20;
    const int typeColW = 52;

    const int knobTotal = 4 * knobSize + 3 * colGap;
    const int totalW = knobTotal + typeColW + colGap;

    const int presetCentreX = presetLcd.getX() + presetLcd.getWidth() / 2;
    const int originX = presetCentreX - totalW / 2;
    const int originY = area.getY();

    int x = originX;
    for (int i = 0; i < 4; ++i)
    {
        knobs[i]->setBounds(x, originY, knobSize, knobSize);
        labels[i]->setBounds(x, originY + knobSize + 2, knobSize, labelH);
        x += knobSize + colGap;

        if (i == 1)
        {
            const int typeY = originY + (knobSize - 34) / 2;
            typeButton.setBounds(x, typeY, typeColW, 34);
            typeLabel.setBounds(x, typeY + 34 + 2, typeColW, labelH);
            x += typeColW + colGap;
        }
    }

    const int knobRowH = knobSize + labelH;
    auto lcdBand = area.withTrimmedTop(knobRowH + 14).removeFromTop(46);

    const int lcdGap = 12;
    const int lcdCount = 4;
    const int lcdW = (lcdBand.getWidth() - (lcdCount - 1) * lcdGap) / lcdCount;
    const int lcdTotalW = lcdW * lcdCount + (lcdCount - 1) * lcdGap;
    const int lcdStartX = lcdBand.getX() + (lcdBand.getWidth() - lcdTotalW) / 2;
    const int lcdY = lcdBand.getY();

    preLcd.setBounds       (lcdStartX + 0 * (lcdW + lcdGap), lcdY, lcdW, 46);
    thresholdLcd.setBounds (lcdStartX + 1 * (lcdW + lcdGap), lcdY, lcdW, 46);
    mixLcd.setBounds       (lcdStartX + 2 * (lcdW + lcdGap), lcdY, lcdW, 46);
    postLcd.setBounds      (lcdStartX + 3 * (lcdW + lcdGap), lcdY, lcdW, 46);

    if (activeEdit == 1)
        preEdit.setBounds(preLcd.getBounds().withTrimmedTop(16).withTrimmedBottom(10));
    else if (activeEdit == 2)
        thresholdEdit.setBounds(thresholdLcd.getBounds().withTrimmedTop(16).withTrimmedBottom(10));
    else if (activeEdit == 3)
        mixEdit.setBounds(mixLcd.getBounds().withTrimmedTop(16).withTrimmedBottom(10));
    else if (activeEdit == 4)
        postEdit.setBounds(postLcd.getBounds().withTrimmedTop(16).withTrimmedBottom(10));
}

juce::AudioProcessorEditor* AlphaFastDistProcessor::createEditor()
{
    return new AlphaFastDistEditor(*this);
}