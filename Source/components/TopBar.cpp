#include "TopBar.h"

TopBar::TopBar() : leftButton ("LeftButton", juce::DrawableButton::ImageFitted), rightButton ("RightButton", juce::DrawableButton::ButtonStyle::ImageFitted)
{
    using namespace juce;


    addAndMakeVisible (leftButton);
    addAndMakeVisible (rightButton);

    centerLabel.setFont (TickLookAndFeel::topBarTextSize);
    centerLabel.setJustificationType (juce::Justification::left);
    addAndMakeVisible (centerLabel);
}

void TopBar::resized()
{
    auto area = getLocalBounds();
    leftButton.setBounds (area.removeFromLeft (getHeight()).reduced (TickLookAndFeel::reducePixels * 2));
    rightButton.setBounds (area.removeFromRight (getHeight()).reduced (TickLookAndFeel::reducePixels * 2));
    if (extendedTopBar)
    {
        // GUI-Säkerhet: Förhindra negativ area om fönstret är extremt litet. 
        // Detta skyddar BottomBar (Transport) från att få negativa dimensioner och krascha JUCE's rendering!
        centerLabel.setBounds (area.removeFromLeft (juce::jmin (250, juce::jmax (0, area.getWidth()))));
        extendedBarArea = area;
    }
    else
    {
        centerLabel.setBounds (area.reduced (15, 0));
        extendedBarArea = {};
    }
}
