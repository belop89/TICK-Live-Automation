#pragma once

#include "JuceHeader.h"
#include "LookAndFeel.h"

class TopBar : public juce::Component
{
public:
    TopBar();
    void resized() override;

    class ClickableLabel : public juce::Label
    {
        std::unique_ptr<juce::Drawable> iconOpen;
        std::unique_ptr<juce::Drawable> iconClosed;
    public:
        ClickableLabel()
        {
            iconOpen = juce::Drawable::createFromImageData (BinaryData::expand_more24px_svg, BinaryData::expand_more24px_svgSize);
            if (iconOpen != nullptr) iconOpen->replaceColour (juce::Colours::black, juce::Colours::white);
            iconClosed = juce::Drawable::createFromImageData (BinaryData::unfold_more_black_24dp_svg, BinaryData::unfold_more_black_24dp_svgSize);
            if (iconClosed != nullptr) iconClosed->replaceColour (juce::Colours::black, juce::Colours::white);
        }
        std::function<void()> onClick {};
        // KOMPILERINGS-FIX: Variabelnamnet 'e' saknades, vilket kraschade bygget!
        void mouseUp (const juce::MouseEvent& e) override
        {
            // UX-Säkerhet: Trigga bara klicket om användaren släpper musknappen INUTI etiketten.
            // (Gör det möjligt att "ångra" ett klick genom att dra bort musen innan man släpper).
            if (onClick != nullptr && getLocalBounds().contains (e.getPosition()))
                onClick();
        }
        void paint (juce::Graphics& g) override
        {
            using namespace juce;
            const auto iconArea = getLocalBounds().removeFromLeft (getHeight()).reduced (TickLookAndFeel::reducePixels * 2);
            g.setColour (Colours::white.withAlpha (0.1f));
            g.fillRoundedRectangle (iconArea.getX() - 5, iconArea.getY() - 2.5f, getWidth() - iconArea.getX() - 5, iconArea.getHeight() + 5, 8.0f);
            auto alpha = isEnabled() ? 1.0f : 0.5f;
        if (isOpen && iconOpen != nullptr) iconOpen->drawWithin (g, iconArea.toFloat(), RectanglePlacement::centred, alpha);
        else if (!isOpen && iconClosed != nullptr) iconClosed->drawWithin (g, iconArea.toFloat(), RectanglePlacement::centred, alpha);
            auto& laf = getLookAndFeel();
            const Font labelFont (laf.getLabelFont (*this));
            g.setColour (findColour (Label::textColourId).withMultipliedAlpha (alpha));
            g.setFont (labelFont);
            const auto textArea = laf.getLabelBorderSize (*this).subtractedFrom (getLocalBounds().withTrimmedLeft (static_cast<int> (getHeight() * 0.8)));
            g.drawFittedText (getText(), textArea, getJustificationType(), jmax (1, (int) ((float) textArea.getHeight() / labelFont.getHeight())), getMinimumHorizontalScale());
            g.setColour (findColour (Label::outlineColourId).withMultipliedAlpha (alpha));
        }
        bool isOpen { false };
    };
    ClickableLabel centerLabel;
    juce::DrawableButton leftButton;
    juce::DrawableButton rightButton;

    // when set, the top-bar also being used for transport instead of bottom area.
    bool extendedTopBar { false };
    juce::Rectangle<int> extendedBarArea {};

private:
};
