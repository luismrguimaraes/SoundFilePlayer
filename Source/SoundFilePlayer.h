/*
  ==============================================================================

   This file is part of the JUCE tutorials.
   Copyright (c) 2020 - Raw Material Software Limited

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
   WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
   PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:             SoundFilePlayer
 version:          1.0.0
 vendor:           JUCE
 website:          http://juce.com
 description:      Plays sound files.

 dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats,
                   juce_audio_processors, juce_audio_utils, juce_core,
                   juce_data_structures, juce_events, juce_graphics,
                   juce_gui_basics, juce_gui_extra
 exporters:        xcode_mac, vs2019, linux_make

 type:             Component
 mainClass:        MainContentComponent

 useLocalCopy:     1

 END_JUCE_PIP_METADATA

*******************************************************************************/


#pragma once


//==============================================================================
class CursorMarker : public juce::Component 
{
public:
    CursorMarker() {};

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::floralwhite);
    }
};
class MainContentComponent   : public juce::AudioAppComponent,
                               public juce::ChangeListener,
                               public juce::Timer
{
public:
    MainContentComponent()
        : state (Stopped),
          thumbnailCache(5),
          thumbnail(512, formatManager, thumbnailCache)
    {
        addAndMakeVisible (&openButton);
        openButton.setButtonText ("Open...");
        openButton.onClick = [this] { openButtonClicked(); };

        addAndMakeVisible (&playButton);
        playButton.setButtonText ("Play");
        playButton.onClick = [this] { playButtonClicked(); };
        playButton.setColour (juce::TextButton::buttonColourId, juce::Colours::green);
        playButton.setEnabled (false);

        addAndMakeVisible (&stopButton);
        stopButton.setButtonText ("Stop");
        stopButton.onClick = [this] { stopButtonClicked(); };
        stopButton.setColour (juce::TextButton::buttonColourId, juce::Colours::red);
        stopButton.setEnabled (false);

        addAndMakeVisible(&cursor);
        cursor.setJustificationType(juce::Justification::centred);

        addAndMakeVisible(&cursorMarker);
        changeCursorPosition();
        
        setSize (500, 350);

        formatManager.registerBasicFormats();
        transportSource.addChangeListener (this);
        thumbnail.addChangeListener(this);

        setAudioChannels (2, 2);
    }

    ~MainContentComponent() override
    {
        shutdownAudio();
    }

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override
    {
        transportSource.prepareToPlay (samplesPerBlockExpected, sampleRate);
    }

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        if (readerSource.get() == nullptr)
        {
            bufferToFill.clearActiveBufferRegion();
            return;
        }

        transportSource.getNextAudioBlock (bufferToFill);
    }

    void releaseResources() override
    {
        transportSource.releaseResources();
    }

    void paint(juce::Graphics& g) override
    {
        int y = vgap + 4 * (h + vgap);

        juce::Rectangle<int> thumbnailBounds(hgap, y, w, getHeight() - vgap*2 -y);

        if (thumbnail.getNumChannels() == 0)
            paintIfNoFileLoaded(g, thumbnailBounds);
        else
            paintIfFileLoaded(g, thumbnailBounds);
    }

    void paintIfNoFileLoaded(juce::Graphics& g, const juce::Rectangle<int>& thumbnailBounds)
    {
        //g.setColour(juce::Colours::darkgrey);
        //g.fillRect(thumbnailBounds);
        g.setColour(juce::Colours::white);
        g.drawFittedText("No File Loaded", thumbnailBounds, juce::Justification::centred, 1);
    }

    void paintIfFileLoaded(juce::Graphics& g, const juce::Rectangle<int>& thumbnailBounds)
    {
        g.setColour(juce::Colours::cyan);

        thumbnail.drawChannels(g,
            thumbnailBounds,
            0.0,
            thumbnail.getTotalLength(),
            1.0f);

    }

    void resized() override
    {
        hgap = 10;
        vgap = 6;
        h = getHeight() / 5 - 2 * vgap;
        w = getWidth() - 2 * hgap;

        int y = vgap;
        openButton.setBounds(hgap, y, w, h); y += h + vgap;
        playButton.setBounds(hgap, y, w, h); y += h + vgap;
        stopButton.setBounds(hgap, y, w, h); y += h + vgap;
        cursor.setBounds(hgap, y, w, h);
    }

    void changeListenerCallback (juce::ChangeBroadcaster* source) override
    {
        if (source == &transportSource)
        {
            if (transportSource.isPlaying())
                changeState (Playing);
            else if ((state == Stopping) || (state == Playing))
                changeState (Stopped);
            else if (state == Pausing)
                changeState (Paused);
        }
        if (source == &thumbnail) repaint();
    }
    void timerCallback() override {
        changeCursorPosition();
    }

private:
    enum TransportState
    {
        Stopped,
        Starting,
        Playing,
        Pausing,
        Paused,
        Stopping
    };

    void changeState (TransportState newState)
    {        
        state = newState;

        switch (state)
        {
            case Stopped:
                playButton.setButtonText ("Play");
                stopButton.setButtonText ("Stop");
                stopButton.setEnabled (false);
                transportSource.setPosition (0.0);
                changeCursorPosition();
                stopTimer();
                break;

            case Starting:
                transportSource.start();
                break;

            case Playing:
                playButton.setButtonText ("Pause");
                stopButton.setButtonText ("Stop");
                stopButton.setEnabled (true);
                startTimer(200);
                break;

            case Pausing:
                transportSource.stop();
                break;

            case Paused:
                playButton.setButtonText ("Resume");
                changeCursorPosition();
                stopTimer();
                break;

            case Stopping:
                transportSource.stop();
                break;
        }        
    }

    void openButtonClicked()
    {
        chooser = std::make_unique<juce::FileChooser> ("Select a Wave file to play...",
                                                       juce::File{},
                                                       "*.wav");
        auto chooserFlags = juce::FileBrowserComponent::openMode
                          | juce::FileBrowserComponent::canSelectFiles;

        chooser->launchAsync (chooserFlags, [this] (const FileChooser& fc)
        {
            auto file = fc.getResult();

            if (file != File{})
            {
                auto* reader = formatManager.createReaderFor (file);

                if (reader != nullptr)
                {
                    auto newSource = std::make_unique<juce::AudioFormatReaderSource> (reader, true);
                    transportSource.setSource (newSource.get(), 0, nullptr, reader->sampleRate);
                    playButton.setEnabled (true);
                    thumbnail.setSource(new juce::FileInputSource(file));
                    readerSource.reset (newSource.release());
                    changeState(Stopped);
                }
            }
        });
    }

    void playButtonClicked()
    {
        if ((state == Stopped) || (state == Paused))
            changeState (Starting);
        else if (state == Playing)
            changeState (Pausing);
    }

    void stopButtonClicked()
    {
        if (state == Paused)
            changeState (Stopped);
        else
            changeState (Stopping);
    }

    void changeCursorPosition() {
        double cursor_position = transportSource.getCurrentPosition();
        double transport_length = transportSource.getLengthInSeconds();
        cursor.setText(juce::String((int)cursor_position / 60 % 60) + ":" + juce::String((int)cursor_position % 60) + " / " +
                       juce::String((int)transport_length / 60 % 60) + ":" + juce::String((int)transport_length % 60),
                       juce::NotificationType::dontSendNotification);

        double cursor_position_percent = cursor_position / transport_length;

        int y = vgap + 4*(h + vgap);
        juce::Rectangle<int> thumbnailBounds(hgap, y, w, getHeight() - vgap * 2 - y);
        cursorMarker.setBounds(thumbnailBounds.getX() + thumbnailBounds.getRight() * cursor_position_percent - 2, thumbnailBounds.getY(), 4, thumbnailBounds.getHeight());
    }

    //==========================================================================
    juce::TextButton openButton;
    juce::TextButton playButton;
    juce::TextButton stopButton;
    juce::Label cursor;
    CursorMarker cursorMarker;

    std::unique_ptr<juce::FileChooser> chooser;

    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;
    TransportState state;
    juce::AudioThumbnailCache thumbnailCache;
    juce::AudioThumbnail thumbnail;

    int hgap;
    int vgap;
    int h;
    int w;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainContentComponent)
};
