Qt implementation of TimelineFX particles preview (Work in progress).

Based on Marmalade loaded with few Qt changes. Should work on every platform however I have tested this only on OSX.

Features:
* Effects loaded from resoures (data.qrc).
* Open eff files directly.

***

How to build (tested with Qt 5.6 - on OSX/Retina).

```bash
git clone https://github.com/ppiecuch/timelinefxcplusplus.git
cd timelinefxcplusplus/tlfx/sample-qt
qmake -o Makefile sample-qt.pro
```

***

Example of preview window:
![Alt text](/tlfx/sample-qt/screens/screen1.png?raw=true "Effect 1")
![Alt text](/tlfx/sample-qt/screens/screen2.png?raw=true "Effect 2")
![Alt text](/tlfx/sample-qt/screens/screen3.png?raw=true "Effect 3")
![Alt text](/tlfx/sample-qt/screens/screen4.jpg?raw=true "Effect 4")
![Alt text](/tlfx/sample-qt/screens/screen5.jpg?raw=true "Texture atlas viewer")
![Alt text](/tlfx/sample-qt/screens/atlas.png?raw=true "Texture atlas example")
