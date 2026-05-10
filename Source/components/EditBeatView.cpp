/*
  ==============================================================================

    EditBeatView.cpp
    Created: 2 Oct 2020 9:05:55am
    Author:  Tal Aviram

  ==============================================================================
*/

#include "EditBeatView.h"
#include "LookAndFeel.h"

#include "JUX/components/ListBoxMenu.h"
#include "utils/UtilityFunctions.h"

EditBeatView::EditBeatView (TickSettings& stateRef, TicksHolder& ticksRef)
    : model (*this), fileChooser ("Import Audio", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.wav;*.wave;*.aif;*.aiff;*.mp3;*.flac", TickUtils::usePlatformDialog()
#if JUCE_IOS
                                                                                                                                                                     ,
                                  false,
                                  getTopLevelComponent() // AUv3 Sandbox requirement
#endif
                                  ),
      beatScrollBack ("back", juce::DrawableButton::ImageFitted),
      beatScrollForward ("fwd", juce::DrawableButton::ImageFitted),
      sampleIcon ("sampleIcon", juce::DrawableButton::ImageFitted),
      sampleSelection ("selectSample", juce::DrawableButton::ImageFitted),
      state (stateRef),
      ticks (ticksRef)

{
    currentColour = juce::Colours::pink;
    samplesList.setColour (juce::ListBox::ColourIds::backgroundColourId, juce::Colours::transparentBlack);
    samplesList.setRowHeight (40);
    samplesList.setModel (&model);
    addAndMakeVisible (samplesList);
    beatVolume.setColour (juce::Slider::ColourIds::backgroundColourId, juce::Colours::black);
    beatVolume.setColour (juce::Slider::ColourIds::trackColourId, currentColour.darker());
    beatVolume.setColour (juce::Slider::ColourIds::thumbColourId, currentColour);
    beatVolume.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    beatVolume.setRange (0.0, 1.0);

    hintText.setFont (juce::Font (30.0f));
    hintText.setJustificationType (juce::Justification::centred);
    hintText.setText ("Select beat(s) to Edit", juce::dontSendNotification);

    addChildComponent (beatLabel);
    addChildComponent (beatVolume);
    addChildComponent (hintText);

    const auto backArrow = jux::getArrowPath ({ 20, 20, 40, 40 }, 3, false, Justification::centred);
    const auto forwardArrow = jux::getArrowPath ({ 20, 20, 40, 40 }, 1, false, Justification::centred);
    juce::DrawablePath d;
    d.setStrokeFill (FillType (Colours::white));
    d.setStrokeType (PathStrokeType (1.0f, PathStrokeType::JointStyle::curved, PathStrokeType::EndCapStyle::rounded));
    d.setPath (backArrow);
    beatScrollBack.setImages (&d);
    d.setPath (forwardArrow);
    beatScrollForward.setImages (&d);
    beatScrollBack.setTitle ("Previous Sample");
    beatScrollForward.setTitle ("Next Sample");
    beatScrollBack.onClick = [this] {
        jassert (selection.size() > 0);
        const auto availableBeats = juce::jlimit(1, (int)TickSettings::kMaxBeatAssignments, state.transport.numerator.get());
        const auto idx = selection.front() - 1;
        updateSelection ({ (idx >= 0 ? idx : (int) (availableBeats - 1)) % availableBeats });
    };
    beatScrollForward.onClick = [this] {
        jassert (selection.size() > 0);
        const auto availableBeats = juce::jlimit(1, (int)TickSettings::kMaxBeatAssignments, state.transport.numerator.get());
        updateSelection ({ (selection.front() + 1) % availableBeats });
    };

    addChildComponent (beatScrollBack);
    addChildComponent (beatScrollForward);

    updateSelection ({});

    ticks.addChangeListener (this);
}

EditBeatView::~EditBeatView()
{
    ticks.removeChangeListener (this);
}

void EditBeatView::resized()
{
    auto area = getLocalBounds();
    hintText.setBounds (getLocalBounds().reduced (10));
    {
        auto topSection = area.removeFromTop (40);
        beatScrollBack.setBounds (topSection.removeFromLeft (40).reduced (0, 10));
        beatScrollForward.setBounds (topSection.removeFromRight (40).reduced (0, 10));
        beatLabel.setBounds (topSection.removeFromLeft (80));
        beatVolume.setBounds (topSection.withTrimmedRight (30));
    }
    samplesList.setBounds (area.reduced (4));
}

void EditBeatView::visibilityChanged()
{
    if (! isVisible())
    {
        listboxMenu.reset();
    }
}

void EditBeatView::paint (juce::Graphics& g)
{
    if (auto* p = getParentComponent())
    {
        // when horizontal, top bar might overlap the top area which is too transparent.
        if (p->getHeight() <= getHeight())
        {
            g.setColour (Colours::black);
            g.fillRect (0, 0, getWidth(), 40);
        }
    }
    using namespace juce;
    Path background;
    background.addRoundedRectangle (0, 0, getWidth(), getHeight(), 6.0f, 6.0f, true, true, false, false);
    g.setGradientFill (ColourGradient (Colours::darkgrey.withBrightness (0.5f).withAlpha (0.2f), 0, 0, Colours::darkgrey.withAlpha (0.2f), 0, getLocalBounds().getCentreY(), false));
    g.fillPath (background);
    g.setColour (Colours::white.withAlpha (0.1f));
    g.strokePath (background, PathStrokeType (1.0f));
}

void EditBeatView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    samplesList.updateContent();
    updateSelection (selection);
}

void EditBeatView::updateSelection (const std::vector<int>& newSelection)
{
    selection = newSelection;

    if (selection.size() == 0)
    {
        beatLabel.setVisible (false);
        beatVolume.setVisible (false);
        hintText.setVisible (true);
        samplesList.setVisible (false);
        return;
    }
    auto& assignment = state.beatAssignments[selection[0]];
    state.selectedEdit = assignment.tickIdx.get();
    currentColour = TickLookAndFeel::sampleColourPallete[assignment.tickIdx.get()];

    beatScrollBack.setVisible (true);
    beatScrollForward.setVisible (true);

    beatVolume.setColour (juce::Slider::ColourIds::trackColourId, currentColour.darker());
    beatVolume.setColour (juce::Slider::ColourIds::thumbColourId, currentColour);

    beatLabel.setFont (juce::Font (20));
    beatLabel.setText ("Beat #" + juce::String (newSelection.front() + 1), juce::dontSendNotification);
    samplesList.setVisible (true);
    hintText.setVisible (false);
    if ((size_t) assignment.tickIdx.get() < ticks.getNumOfTicks())
    {
        beatVolume.getValueObject().referTo (assignment.gain.getPropertyAsValue());
        samplesList.selectRow (assignment.tickIdx.get());
        beatLabel.setVisible (true);
        beatVolume.setVisible (true);
        if (state.view.showEditSamples.getValue())
        {
            state.view.showEditSamples.setValue (false);
            state.view.showEditSamples.setValue (true);
        }
    }
    else
    {
        samplesList.deselectAllRows();
        beatLabel.setVisible (false);
        beatVolume.setVisible (false);
    }
    if (onBeatUpdate)
        onBeatUpdate (selection);
}

/// SamplesModel
EditBeatView::SamplesModel::SamplesModel (EditBeatView& o)
    : owner (o)
{
}

int EditBeatView::SamplesModel::getNumRows()
{
    return static_cast<int> (owner.ticks.getNumOfTicks()) + 1;
}

void EditBeatView::SamplesModel::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    // no need to draw, also samples might be updated
    // as we draw...
    if (owner.state.view.showEditSamples.getValue())
        return;

    const auto isLast = isLastRow (rowNumber);
    if (rowIsSelected)
        g.fillAll (owner.currentColour);

    g.setColour (rowIsSelected ? juce::Colours::black.withAlpha (0.9f) : juce::Colours::white);
    g.setFont (juce::Font (30.0f));

    g.drawFittedText (isLast ? "Add new sample..." : owner.ticks[(size_t) rowNumber].getName(), 20, 0, width - 20, height, juce::Justification::centredLeft, 1);
    if (! rowIsSelected)
    {
        g.setColour (juce::Colours::white.withAlpha (0.4f));
        g.drawHorizontalLine (height - 1, 20, width - 20);
    }
}

juce::Component* EditBeatView::SamplesModel::refreshComponentForRow (int rowNumber, bool /*isRowSelected*/, juce::Component* existingComponentToUpdate)
{
    if (existingComponentToUpdate == nullptr)
        existingComponentToUpdate = new SampleOption (owner);

    auto l = static_cast<SampleOption*> (existingComponentToUpdate);
    l->row = rowNumber;

    return existingComponentToUpdate;
}

EditBeatView::SamplesModel::SampleOption::SampleOption (EditBeatView& e) : owner (e)
{
}

void EditBeatView::SamplesModel::SampleOption::paint (juce::Graphics& g)
{
    using namespace juce;
    if (owner.model.isLastRow (row))
        return;

    {
        // GUI-Optimering: Ladda SVG-ikonen en enda gång i minnet istället för varje bildruta!
        static std::unique_ptr<juce::Drawable> img = [] {
            auto d = juce::Drawable::createFromImageData (BinaryData::more_vert24px_svg, BinaryData::more_vert24px_svgSize);
            d->replaceColour (juce::Colours::black, juce::Colours::white);
            return d;
        }();
        if (img != nullptr)
            img->drawWithin (g, getLocalBounds().removeFromRight (getHeight()).reduced (10).toFloat(), RectanglePlacement::centred, 1.0f);
    }
}

void EditBeatView::SamplesModel::SampleOption::mouseDown (const juce::MouseEvent&)
{
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Edit Sample...");
        menu.addItem (2, "Replace Sample...");
        menu.addItem (3, "Delete Sample...");
        menu.addSeparator();
        menu.addItem (4, "Set sample to this beat and onward...");
        auto options = juce::PopupMenu::Options().withParentComponent (owner.getParentComponent()).withTargetScreenArea (getScreenBounds().removeFromRight (getHeight()));
        menu.showMenuAsync (options, [safeThis = juce::Component::SafePointer<SampleOption>(this)] (int value) {
            if (safeThis == nullptr) return;
            switch (value)
            {
                case 1:
                    safeThis->owner.state.view.showEditSamples = true;
                    break;
                case 2:
                {
                    // TODO: DRY with code below for adding new samples
                    auto addSamplesMenu = safeThis->owner.getAddSamplesMenu (safeThis->row);
                    safeThis->owner.listboxMenu.reset (new jux::ListBoxMenu());
                    safeThis->owner.listboxMenu->setRowHeight (50);
                    safeThis->owner.listboxMenu->setMenuFromPopup (std::move (addSamplesMenu));
                    safeThis->owner.listboxMenu->setShouldCloseOnItemClick (true);
                    safeThis->owner.getParentComponent()->addAndMakeVisible (safeThis->owner.listboxMenu.get());
                    safeThis->owner.listboxMenu->setBounds (safeThis->owner.getParentComponent()->getLocalBounds());
                    safeThis->owner.listboxMenu->toFront (true);
                    safeThis->owner.listboxMenu->setOnRootBackToParent ([safeThis]() {
                        if (safeThis != nullptr) safeThis->owner.getParentComponent()->removeChildComponent (safeThis->owner.listboxMenu.get());
                    });
                }
                break;
                case 3:
                {
                    if (! safeThis->owner.selection.empty())
                    {
                        auto& assignment = safeThis->owner.state.beatAssignments[safeThis->owner.selection.front()];
                        assignment.tickIdx.setValue (std::max (0, assignment.tickIdx.get() - 1), nullptr);
                        safeThis->owner.ticks.removeTick (safeThis->row);
                    }
                    break;
                }
                case 4:
                {
                    const auto currentSelection = safeThis->owner.selection[0];
                    for (auto i = currentSelection; i < TickSettings::kMaxBeatAssignments; ++i)
                        safeThis->owner.state.beatAssignments[i].tickIdx = safeThis->row;
                    break;
                }
                default:
                    break;
            }
        });
    }
}

juce::PopupMenu EditBeatView::getAddSamplesMenu (const int replaceIndex)
{
    using namespace juce;
    PopupMenu menu;
    menu.addSectionHeader ("Factory Samples");
    const auto factorySamples = TickUtils::getFactorySamples();
    factorySamples->sortEntriesByFilename();
    for (auto i = 0; i < factorySamples->getNumEntries(); i++)
    {
        auto& item = *factorySamples->getEntry (i);
        String sampleName (item.filename.substring (0, item.filename.length() - 4));
        menu.addItem (sampleName, [safeThis = juce::Component::SafePointer<EditBeatView>(this), sampleName, replaceIndex] {
            if (safeThis == nullptr) return;
            auto samples = TickUtils::getFactorySamples();
            auto* entry = samples->getEntry (sampleName + ".wav");
        if (entry != nullptr)
        {
            auto newTick = std::unique_ptr<Tick> (safeThis->ticks.importAudioStream (sampleName, std::unique_ptr<InputStream> (samples->createStreamForEntry (*entry))));
            safeThis->setNewImportedSample (replaceIndex, std::move (newTick));
        }
        });
    }
    menu.addSeparator();
    menu.addItem ("Import Audio File...", TickUtils::canImport(), false, [safeThis = juce::Component::SafePointer<EditBeatView>(this), replaceIndex]()
                  { 
                      if (safeThis == nullptr) return;
                      safeThis->fileChooser.launchAsync (FileBrowserComponent::FileChooserFlags::openMode | FileBrowserComponent::FileChooserFlags::canSelectFiles,
                                             [safeThis, replaceIndex] (const FileChooser& chooser)
                                             {
                                                 if (safeThis == nullptr) return;
#if JUCE_ANDROID
                                                 if (! chooser.getURLResult().isEmpty())
#else
                                         if (chooser.getResult().existsAsFile())
#endif
                                                 {
#if JUCE_IOS || JUCE_ANDROID
                                                     auto newTick = std::unique_ptr<Tick> (
                                                         safeThis->ticks.importURL (chooser.getURLResult()));
#else
                                                                                                        auto newTick = std::unique_ptr<Tick> (safeThis->ticks.importAudioFile (chooser.getResult()));
#endif
                                                     safeThis->setNewImportedSample (replaceIndex, std::move (newTick));
                                                 }
                                                 safeThis->getTopLevelComponent()->repaint();
                                             }); });
    return menu;
}

void EditBeatView::setNewImportedSample (const int replaceIndex, std::unique_ptr<Tick> newTick)
{
    if (replaceIndex == -1)
    {
        ticks.addTick (std::move (newTick));
        state.beatAssignments[selection.front()].tickIdx = { static_cast<int> (ticks.getNumOfTicks() - 1) };
        updateSelection (selection);
    }
    else
        ticks.replaceTick (replaceIndex, std::move (newTick));
    if (listboxMenu != nullptr && getParentComponent() != nullptr)
        getParentComponent()->removeChildComponent (listboxMenu.get());
}

bool EditBeatView::SamplesModel::SampleOption::hitTest (int x, int /*y*/)
{
    if (owner.model.isLastRow (row))
        return false;
    // more options area...
    return x > getWidth() - getHeight() * 0.6;
}

inline bool EditBeatView::SamplesModel::isLastRow (int row) const
{
    return (size_t) row >= owner.ticks.getNumOfTicks();
}

void EditBeatView::SamplesModel::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    if (! isLastRow (row) && owner.selection.size() > 0)
    {
        owner.state.beatAssignments[owner.selection.front()].tickIdx = row;
        owner.updateSelection (owner.selection);
    }
    else
    {
        owner.updateSelection (owner.selection);
        auto menu = owner.getAddSamplesMenu();
        owner.listboxMenu.reset (new jux::ListBoxMenu());
        owner.listboxMenu->setShouldCloseOnItemClick (true);
        owner.listboxMenu->setRowHeight (50);
        owner.listboxMenu->setMenuFromPopup (std::move (menu));
        owner.listboxMenu->setOnRootBackToParent ([safeOwner = juce::Component::SafePointer<EditBeatView>(&owner)]() {
            if (safeOwner != nullptr) safeOwner->getParentComponent()->removeChildComponent (safeOwner->listboxMenu.get());
        });
        owner.getParentComponent()->addAndMakeVisible (owner.listboxMenu.get());
        owner.listboxMenu->setBounds (owner.getParentComponent()->getLocalBounds());
        owner.listboxMenu->toFront (true);
    }
}
