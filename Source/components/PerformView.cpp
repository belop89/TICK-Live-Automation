/*
  ==============================================================================

    PerformView.cpp
    Created: 27 Sep 2020 5:53:58pm
    Author:  Tal Aviram

  ==============================================================================
*/

#include "PerformView.h"
#include "LookAndFeel.h"

#include "EditBeatView.h"
#include "utils/SamplesPaint.h"
#include "../PluginProcessor.h"

PerformView::PerformView (TickSettings& stateToLink, TicksHolder& ticksToLink, SamplesPaint& paint)
    : state (stateToLink), ticks (ticksToLink), samplesPaint (paint), editView (std::make_unique<EditBeatView> (state, ticks))
{
    using namespace juce;

    setInterceptsMouseClicks (false, true); // needed for top bar to work
    viewport.setScrollBarsShown (true, false);
    viewport.setViewedComponent (&beatsView);
    addAndMakeVisible (viewport);

    topBar.tempo.setText (state.transport.bpm.getPropertyAsValue().toString(), dontSendNotification);
    topBar.tempo.getTextValue().referTo (state.transport.bpm.getPropertyAsValue());
    topBar.num.getTextValue().referTo (state.transport.numerator.getPropertyAsValue());
    topBar.num.setText (juce::String (state.transport.numerator.get()), dontSendNotification);
    topBar.denum.getTextValue().referTo (state.transport.denumerator.getPropertyAsValue());
    topBar.denum.setText (juce::String (state.transport.denumerator.get()), dontSendNotification);
    addAndMakeVisible (topBar);

    editView->onBeatUpdate = [this] (std::vector<int>& selection) {
        for (auto&& i : selection)
            selectionChanged (i, false);
    };

    beatsView.addMouseListener (this, false);
    topBar.tapMode.addMouseListener (this, true);
    // draggable labels...
    tempoDrag.onStep = [this] (bool isUp) {
        const float bpm = state.transport.bpm.get();
        const int step = isUp ? +1 : -1;
        state.transport.bpm.setValue (bpm + step, nullptr);
    };

    numDrag.onStep = [this] (bool isUp) {
        const int num = state.transport.numerator.get();
        const int step = isUp ? +1 : -1;
        state.transport.numerator.setValue (jlimit (1, TickSettings::kMaxBeatAssignments, num + step), nullptr);
    };

    denumDrag.onStep = [this] (bool isUp) {
        const int denum = state.transport.denumerator.get();
        const int step = isUp ? +1 : -1;
        state.transport.denumerator.setValue (jlimit (1, 128, denum + step), nullptr);
    };

    topBar.tempo.addMouseListener (this, true);
    topBar.num.addMouseListener (this, true);
    topBar.denum.addMouseListener (this, true);

    addChildComponent (editView.get());

    juce::Desktop::getInstance().getAnimator().addChangeListener (this);
}

PerformView::~PerformView()
{
    juce::Desktop::getInstance().getAnimator().removeChangeListener (this);
}

void PerformView::selectionChanged (const int index, const bool propogateToEditView)
{
    // TODO: remove unused multi-selection API left over? clean up code!
    // this actually supports multi-selection but unused for this UX
    if (propogateToEditView)
    {
        std::vector<int> newSelection { index };
        editView->updateSelection (newSelection);
    }
    for (size_t idx = 0; idx < beats.size(); idx++)
    {
        jassert (idx < 64);
        const bool shouldBeSelected = (idx == (size_t) index);
        
        if (beats[idx]->isSelected != shouldBeSelected)
        {
            beats[idx]->isSelected = shouldBeSelected;
            beats[idx]->repaint(); // GUI-Fix: Tvinga opritning så gamla slaget tappar sin markering!
        }
    }
}

int PerformView::getEditViewHeight()
{
    return std::min<int> (200, getHeight());
}

void PerformView::resized()
{
    const auto editViewHeight = getEditViewHeight();
    auto area = getLocalBounds();

    topBar.setBounds (area.removeFromTop (TickLookAndFeel::barHeight));
    if (isEditMode && editView->isVisible())
    {
        // FIX: Använd getHeight() istället för getBottom() för korrekta lokala koordinater!
        juce::Rectangle<int> showing (0, getHeight() - editViewHeight, getWidth(), editViewHeight);
        if (! isAnimatingEditView.load())
            editView->setBounds (showing);
        area.removeFromBottom (editViewHeight);
    }
    viewport.setBounds (area);

    const auto isVertical = state.isVertical.get();
    beatsInRow = juce::jlimit (1, 8, juce::jmin (state.transport.numerator.get(), state.transport.denumerator.get()));
    // GUI-Säkerhet: Förhindra negativa dimensioner vid extremt snabb fönster-omskalning (kraschar FlexBox!)
    const int availableSpace = (isVertical ? area.getHeight() : area.getWidth()) - 2 * kMargin;
    const int beatSize = std::max(1, (availableSpace / beatsInRow) - 2 * kMargin);
    const auto beatHeight = std::max(1, (int) std::min (beatSize, area.getHeight() - 3 * kMargin));
    const auto itemWidth = std::max(1, isVertical ? area.getWidth() - 2 * kMargin - viewport.getScrollBarThickness() : beatSize);
    const auto numOfBeats = state.transport.numerator.get();
    // GUI-Optimering/Fix: Cast 'numOfBeats' till float innan division! Annars gör C++ integer-division (ex. 5/4 = 1)
    // vilket leder till att de sista slagen klipps bort och blir osynliga i oregelbundna taktarter.
    const int safeWidth = std::max(1, area.getWidth() - viewport.getScrollBarThickness());
    beatsView.setBounds (area.withHeight (std::max (area.getHeight(), (isVertical ? state.transport.numerator.get() : (int) std::ceil ((float)numOfBeats / beatsInRow)) * (beatSize + 2 * kMargin) + kMargin)).withWidth (safeWidth));
    juce::FlexBox fb (isVertical ? juce::FlexBox::Direction::column : juce::FlexBox::Direction::row, juce::FlexBox::Wrap::wrap, juce::FlexBox::AlignContent::flexStart, juce::FlexBox::AlignItems::center, juce::FlexBox::JustifyContent::flexStart);
    
    const float fBeatHeight = static_cast<float> (beatHeight);
    const float fItemWidth = static_cast<float> (itemWidth);
    const float fMargin = static_cast<float> (kMargin);

    for (auto& beat : beats)
    {
        juce::FlexItem newItem (*beat);
        fb.items.add (newItem.withFlex (1.0f)
                          .withMinHeight (fBeatHeight)
                          .withMinWidth (fItemWidth)
                          .withMaxHeight (fBeatHeight)
                          .withMaxWidth (fItemWidth)
                          .withMargin (juce::FlexItem::Margin (fMargin)));
    }
    fb.performLayout (beatsView.getLocalBounds());
}

void PerformView::update (double currentPos)
{
    // GUI-Optimering: Läs av ValueTree-parametrar EN GÅNG i början av uppdateringen.
    // Eftersom update() körs 50 gånger i sekunden undviker vi att låsa och söka i trädet 
    // upprepade gånger per frame, vilket gör gränssnittet blixtsnabbt och sparar CPU!
    const int currentNum = state.transport.numerator.get();
    const int currentDenum = state.transport.denumerator.get();
    const bool isPlaying = state.transport.isPlaying.get();
    const float currentBpm = state.transport.bpm.get();

    // Säkerhetsspärr: Förhindra Memory Access Violation om DAW:en skickar extrema taktartsvärden (>64)
    const auto numOfBeats = juce::jlimit(1, (int)TickSettings::kMaxBeatAssignments, currentNum);
    
    // GUI-Optimering/Fix: Jämför INTE beatsInRow direkt med denumrator! Vid udda taktarter (ex 3/8) 
    // låstes de i ojämlikhet (3 != 8), vilket orsakade en evighetsloop där gränssnittet raderades 
    // och heap-allokerades 50 gånger i sekunden!
    const int expectedBeatsInRow = juce::jlimit (1, 8, juce::jmin (currentNum, currentDenum));
    const bool layoutChanged = (beatsInRow != expectedBeatsInRow);
    
    if ((size_t) numOfBeats != beats.size() || layoutChanged)
    {
        if ((size_t) numOfBeats != beats.size())
        {
            beats.clear();
            for (auto beat = 0; beat < numOfBeats; beat++)
            {
                auto beatView = std::make_unique<BeatView> (*this, beat);
                beatView->setEnabled (isEditMode);
                beats.push_back (std::move (beatView));
                beatsView.addAndMakeVisible (beats.back().get());
            }
        }
        resized();
    }
    // GUI-Fix: Lägg till epsilon (0.0001) för att förhindra att flyttalsavrundning från 
    // atomics orsakar att animationen "blinkar till" på föregående slag just innan nästa slag börjar!
    const auto beatPosExclusive = std::max<double> (0, currentPos - 1.0 + 0.0001);
    const auto currentBeat = floor (beatPosExclusive);
    const auto barPos = beatPosExclusive - currentBeat;
    
    for (size_t num = 0; num < (size_t) numOfBeats; num++)
    {
        auto& beat = *beats[num];
        const bool wasOn = beat.isOn;
        const bool wasCurrent = beat.isCurrent;
        const float oldPos = beat.relativePos;

        beat.isOn = isPlaying && (num < currentBeat);
        beat.isCurrent = isPlaying && (num == currentBeat);
        beat.relativePos = static_cast<float> (beat.isCurrent ? barPos : 0.0f);

        // GUI-Optimering: Förhindra onödiga repaints för sub-pixel-förändringar. Sparar massivt 
        // med GPU/CPU eftersom små flyttals-differenser (< 0.005) ändå inte syns för blotta ögat!
        if (wasOn != beat.isOn || wasCurrent != beat.isCurrent || std::abs(beat.relativePos - oldPos) > 0.005f)
            beat.repaint();
        if (isPlaying && ! isEditMode && beat.isCurrent && (viewport.getViewArea().getBottom() < beat.getY() || viewport.getViewArea().getY() > beat.getY()))
        {
            viewport.setViewPosition (0, beat.getY());
        }
    }
    const bool isStandalone = ! state.useHostTransport.get();
    
    // GUI-Optimering: Använd komponentens inbyggda property-minne för att slippa anropa 
    // getText() som annars skapar heap-allokeringar (juce::String) 50 gånger i sekunden per instans!
    static const juce::Identifier lastBpmId("lastBpm");
    const float lastBpm = topBar.tempo.getProperties().getWithDefault(lastBpmId, -1.0f);
    
    static const juce::Identifier lastNumId("lastNum");
    const int lastNum = topBar.num.getProperties().getWithDefault(lastNumId, -1);

    static const juce::Identifier lastDenumId("lastDenum");
    const int lastDenum = topBar.denum.getProperties().getWithDefault(lastDenumId, -1);

    if (topBar.tempo.isEnabled() != isStandalone || std::abs(currentBpm - lastBpm) > 0.01f || currentNum != lastNum || currentDenum != lastDenum)
    {
        topBar.tempo.getProperties().set(lastBpmId, currentBpm);
        topBar.num.getProperties().set(lastNumId, currentNum);
        topBar.denum.getProperties().set(lastDenumId, currentDenum);
        
        topBar.tempo.setDescription (juce::String (currentBpm) + "BPM");
        topBar.num.setDescription (juce::String (currentNum) + " beats numerator");
        topBar.denum.setDescription (juce::String (currentDenum) + " beats denumerator");
        topBar.tempo.setEnabled (isStandalone);
        topBar.num.setEnabled (isStandalone);
        topBar.denum.setEnabled (isStandalone);
    }
}

void PerformView::setEditMode (const bool newMode)
{
    isEditMode = newMode;
    for (auto& beat : beats)
    {
        // clears selection when not in edit mode
        if (! isEditMode)
        {
            beat->isSelected = false;
            beat->setEnabled (false);
        }

        beat->setEnabled (isEditMode);
    }
    editView->updateSelection ({ 0 });
    // FIX: Se till att menyn alltid ritas under animationen och ligger överst (Z-order)
    editView->setVisible (true);
    editView->toFront (false);
    const auto height = getEditViewHeight();
    juce::Rectangle<int> hidden (0, getHeight(), getWidth(), height);
    juce::Rectangle<int> showing (0, getHeight() - height, getWidth(), height);
    editView->setBounds (isEditMode ? hidden : showing);
    isAnimatingEditView = true;
    resized();
    juce::Desktop::getInstance().getAnimator().animateComponent (editView.get(), isEditMode ? showing : hidden, 1.0f, 150, true, 1.0, 1.0);
}

PerformView::BeatView::BeatView (PerformView& parent, const int idx)
    : index (idx), owner (parent)
{
    setValue (owner.state.beatAssignments[idx].gain.get());
    getValueObject().referTo (owner.state.beatAssignments[idx].gain.getPropertyAsValue());
    setTextBoxStyle (TextBoxBelow, false, 0, 0);
    setRange (0.0f, 1.0f);
    setSliderStyle (SliderStyle::RotaryHorizontalVerticalDrag);
}

bool PerformView::BeatView::isInterestedInFileDrag (const juce::StringArray& files)
{
    if (files.size() != 1)
        return false;

    auto file = juce::File (files[0]);

    if (file.isDirectory())
        return false;

    if (! file.hasFileExtension (".wave;wav;aif;aiff;mp3;flac"))
        return false;

    return true;
}
void PerformView::BeatView::filesDropped (const juce::StringArray& files, int /*x*/, int /*y*/)
{
    auto possibleTick = std::unique_ptr<Tick> (owner.ticks.importAudioFile (juce::File (files[0])));
    if (possibleTick)
    {
        owner.ticks.addTick (std::move (possibleTick));
        owner.state.beatAssignments[index].tickIdx = static_cast<int> (owner.ticks.getNumOfTicks() - 1);
    }
    hasDraggedItem = false;
    repaint();
}

void PerformView::BeatView::fileDragEnter (const juce::StringArray&, int /*x*/, int /*y*/)
{
    hasDraggedItem = true;
    repaint();
}
void PerformView::BeatView::fileDragExit (const juce::StringArray&)
{
    hasDraggedItem = false;
    repaint();
}

void PerformView::BeatView::paint (juce::Graphics& g)
{
    jassert (index > -1);
    constexpr auto cornerSize = 3.0f;
    auto& assignment = owner.state.beatAssignments[index];
    const auto tickIndex = assignment.tickIdx.get();
    const auto gain = assignment.gain.get();
    const auto gainedColour = TickLookAndFeel::sampleColourPallete[tickIndex].withAlpha (gain);
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colours::white.withAlpha (0.3f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), cornerSize, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRoundedRectangle (bounds, cornerSize);

    if (isOn || hasDraggedItem)
    {
        g.setColour (gainedColour);
        g.fillRoundedRectangle (bounds, cornerSize);
    }
    if (isCurrent || hasDraggedItem)
    {
        g.setColour (gainedColour);
        g.fillRoundedRectangle (0.0f, bounds.getY(), bounds.getWidth() * relativePos, bounds.getHeight(), cornerSize);
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), cornerSize, cornerSize);
    }
    if (isEnabled() && isSelected)
    {
        g.setColour (TickLookAndFeel::sampleColourPallete[tickIndex]);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), cornerSize, cornerSize);
        g.setColour (juce::Colours::white);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), cornerSize, cornerSize);
    }
    if ((owner.isEditMode || owner.state.showWaveform.get()) && tickIndex >= 0 && tickIndex < (int) owner.ticks.getNumOfTicks())
        owner.samplesPaint.drawTick (g, getLocalBounds().reduced (10), tickIndex, assignment.gain.get(), juce::Colours::white.withAlpha (isCurrent && ! owner.state.showBeatNumber.get() ? 1.0f : isSelected || owner.state.showBeatNumber.get() ? 0.7f
                                                                                                                                                                                                                                                 : 0.3f));

    if (! owner.isEditMode && owner.state.showBeatNumber.get())
    {
        const auto curBounds = getLocalBounds();
        g.setColour (juce::Colours::white.withAlpha (isCurrent ? 1.0f : 0.1f));
        g.setFont (juce::Font (std::max<int> (4, curBounds.getHeight() - 20)));
        g.drawFittedText (juce::String (index + 1), curBounds, juce::Justification::centred, 1);
    }

    if (hasDraggedItem)
    {
        g.setColour (juce::Colours::white);
        g.drawFittedText ("DROP HERE", getLocalBounds(), juce::Justification::centred, 1);
    }
}

void PerformView::mouseDown (const juce::MouseEvent& e)
{
    if (e.originalComponent == &topBar.tapMode)
    {
        topBar.tapMode.setColour (juce::Label::backgroundColourId, TickLookAndFeel::Colours::wood);
        
        // GUI-Optimering: Trigga den globala DSP-parametern direkt istället för att beräkna 
        // tempot lokalt. Detta ger oss fas-alignering och samma debounce som MIDI-pedalen!
        if (auto* editor = findParentComponentOfClass<juce::AudioProcessorEditor>())
        {
            if (auto* proc = dynamic_cast<TickAudioProcessor*>(editor->getAudioProcessor()))
                if (auto* param = proc->getAPVTS().getParameter (TickAudioProcessor::kTapTempoButtonID))
                    param->setValueNotifyingHost (1.0f);
        }
        return;
    }

    if (e.originalComponent == &topBar.tempo)
    {
        tempoDrag.dragStep = 0;
        return;
    }

    if (e.originalComponent == &topBar.num)
    {
        numDrag.dragStep = 0;
        return;
    }

    if (e.originalComponent == &topBar.denum)
    {
        denumDrag.dragStep = 0;
        return;
    }
}

void PerformView::mouseUp (const juce::MouseEvent& e)
{
    if (e.originalComponent == &beatsView && isEditMode)
    {
        state.view.isEdit.setValue (false);
        return;
    }

    if (e.originalComponent == &topBar.tapMode)
    {
        topBar.tapMode.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    }
}

void PerformView::mouseDrag (const juce::MouseEvent& e)
{
    if (e.originalComponent == &topBar.tempo)
    {
        tempoDrag.handleDrag (e);
        return;
    }
    if (e.originalComponent == &topBar.num)
    {
        numDrag.handleDrag (e);
        return;
    }
    if (e.originalComponent == &topBar.denum)
    {
        denumDrag.handleDrag (e);
        return;
    }
}

void PerformView::BeatView::mouseDown (const juce::MouseEvent& e)
{
    if (! isEnabled())
        return;
    owner.selectionChanged (index);
    repaint();
    // UI-Fix: Ta bort OS-begränsningen helt! Detta är en Slider, om vi blockerar mouseDown 
    // på mobila enheter går det inte att justera volymen för individuella slag på iPad/Android.
    juce::Slider::mouseDown (e);
}

static void setupSigLabel (juce::Label& l)
{
    l.setFont (juce::Font (30.0));
    l.setKeyboardType (juce::TextEditor::VirtualKeyboardType::phoneNumberKeyboard);
    l.setJustificationType (juce::Justification::centred);
    l.onEditorShow = [&l] {
        l.getCurrentTextEditor()->setInputRestrictions (5, "0123456789");
    };
}

PerformView::TopBar::TopBar()
{
    setInterceptsMouseClicks (false, true);
    tempoLabel.setText ("BPM", juce::dontSendNotification);
    tempoLabel.setFont (juce::Font (30.0));
    tempoLabel.setJustificationType (juce::Justification::centred);
    tempo.setKeyboardType (juce::TextEditor::VirtualKeyboardType::numericKeyboard);
    tempo.setFont (juce::Font (30.0));
    tempo.setJustificationType (juce::Justification::centred);
    tempo.onEditorShow = [this] {
        tempo.getCurrentTextEditor()->setInputRestrictions (6, "0123456789.");
        tempo.getCurrentTextEditor()->onTextChange = [this] {
            auto* editor = tempo.getCurrentTextEditor();
            if (editor->getText().length() > 3 && ! editor->getText().contains ("."))
                editor->deleteBackwards (false);
        };
    };
    setupSigLabel (sigDivider);
    setupSigLabel (num);
    setupSigLabel (denum);
    sigDivider.setText ("/", juce::dontSendNotification);
    sigDivider.setAccessible (false);

    tapMode.setFont (juce::Font (30.0));
    tapMode.setJustificationType (juce::Justification::centred);
    tapMode.setText ("TAP", juce::dontSendNotification);
    tapMode.setDescription ("Tap Tempo Button");

    tempo.setEditable (true);
    num.setEditable (true);
    denum.setEditable (true);

    addAndMakeVisible (tempo);
    addAndMakeVisible (tempoLabel);
    addAndMakeVisible (sigDivider);
    addAndMakeVisible (num);
    addAndMakeVisible (denum);
    addAndMakeVisible (tapMode);
}

void PerformView::TopBar::resized()
{
    auto area = getLocalBounds();
    const int bpmWidth = (int) tempo.getFont().getStringWidth ("999.99") + 10;
    tempo.setBounds (area.removeFromLeft (bpmWidth));
    auto signatureArea = area.removeFromRight (bpmWidth);
    auto sigWidth = bpmWidth / 2;
    num.setBounds (signatureArea.removeFromLeft (sigWidth));
    denum.setBounds (signatureArea.removeFromRight (sigWidth));
    sigDivider.setBounds (signatureArea.expanded (30, 0));
    constexpr auto kWidth = 100;
    auto tapBounds = juce::Rectangle<int> (getLocalBounds().getCentreX() - kWidth / 2, 0, kWidth, getHeight());
    tapMode.setBounds (tapBounds.reduced (0, 4));
}

void PerformView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // FIX: Trigga bara layout-förändringar (resized) om menyn FAKTISKT höll på att animeras.
    // Detta förhindrar att pluginen stjäl fönsterfokus från DAW:ens "File"-menyer!
    if (isAnimatingEditView.load() && ! juce::Desktop::getInstance().getAnimator().isAnimating (editView.get()) && getParentComponent())
    {
        isAnimatingEditView = false;
        editView->setVisible (isEditMode);
        resized();
    }
}
