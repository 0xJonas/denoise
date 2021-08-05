# Denoise

Audio Noise reduction using a Wiener filter and Portaudio.

## Building using GCC and Make

1. Download and install [Portaudio](http://www.portaudio.com/)

2. Make sure that the Portaudio headers can be found by the compiler and the library by the linker.

3. Download the contents of this repository.

4. Navigate to the directory where you downloaded `denoise` amd run `make`.

## Running

1. Run `denoise`

2. The app will display a list of available audio devices. Select an input device by entering the corresponding index. Input devices are marked with a `>` in the device list.

3. Select an output device by entering the corresponding index. Output devices are marked with a `<`.

4. `denoise` is now processing the audio. Press `ENTER` to stop the app.

The original purpose of `denoise` is to reduce microphone noise. A good way to do that is by using some form of virtual audio cable.
