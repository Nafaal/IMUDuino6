# IMUDuino6

## 概要

ESP8366,3軸ジャイロセンサー,3軸加速度センサー,気象センサー,フルカラーLED,有機ELディスプレイを統合した
[7IMUduino](http://www.papa.to/kinowiki/index.php/Products/7IMUduino) のためのファームウェアです。
全ての機能をテストするために盛りだくさんとなっております。

ESP8266モジュールを組み込んだArduino IDE(1.6.9)でビルドが可能です。

## フィーチャー

- 初回起動時のキャリブレーションモードではジャイロセンサーの誤差を自動修正します
- クライアントモードではセンサーデータをUDPパケットにして所定のサーバへ送信します
- サーバモードではスマホ等のWebブラウザからSSID,センサーオフセット,サーバアドレスを設定できます
- 有機ELディスプレイに周期的に温度, 気圧, 湿度を表示します
- 有機ELディスプレイに周期的に傾き(Yaw,Roll,Pitch)を表示します
- フルカラーLEDに内部ステータスを表示します

## インストール(Arduino)

USBシリアル変換アダプター用のドライバは適切なものをインストールしておいてください。

ビルドには [Arduino IDE 1.6.9](https://www.arduino.cc/en/Main/Software) が必要です。

[ESP8266のボードデータ](https://github.com/esp8266/Arduino) を参照して、
Arduino IDEのボードマネージャに[Staging version](http://arduino.esp8266.com/staging/package_esp8266com_index.json) を設定します。

ビルドには以下のArduinoライブラリが必要です。

- https://github.com/Makuna/NeoPixelBus/tree/master
- https://github.com/squix78/esp8266-oled-ssd1306/tree/2.0.2
- https://github.com/sparkfun/SparkFun_BME280_Arduino_Library
- https://github.com/jrowberg/i2cdevlib

一部パッチを当てる必要があるのでlibinstall.shを参考にしてください。
ライブラリはデフォルトでは各ホームディレクトリのArduino/librariesディレクトリに置きます。

imuduino6.inoをクリックするとArduino IDEが起動します。ボードタイプは以下のように設定してください。

- Board: Generic ESP8266 module
- FlashMode: DIO
- FlashFreq: 40MHz
- CPUFreq: 80MHz
- FlashSize: 512K
- DebugPort: Disabled
- DebugLevel: None
- ResetMethod: ck
- UploadSpeed: 115200
- SerialPort: 適切なシリアルポート
- 書き込み装置: AVR ISP

7IMUduinoのIO0ボタンを押したまま、Arduino IDEの書き込みボタンを押せばビルドされて書き込まれるはずです。

## インストール(Processing)

IMUduino6のUDPパケットを受信するためにUDPTeapot.pdeというデモプログラムが含まれていますが、
これを動作させるためにはWindows/Macなどで動作する統合開発環境[Processing](https://processing.org/) が必要です。

またUDPTeapot.pdeを動作させるためには以下のProcessingライブラリが必要です。

- http://toxiclibs.org/downloads/
- http://ubaa.net/shared/processing/udp/

ライブラリはデフォルトでは各ホームディレクトリのProcessing/librariesディレクトリに置きます。

UDPTeapot.pdeをダブルクリックすればProcessingが起動します。


## 動作確認(キャリブレーション)

ボードの初回起動時は自動的にキャリブレーションモードになります。LEDはパープルに常時点灯しますので判別できるはずです。
有機ELには「Calibration...」と表示されています。

この状態で、可能限りボードが水平になるように設置して、静かにIO0ボタンを押すと「Start...」と表示されてキャリブレーションが
開始されます。キャリブレーション中はLEDは水色に常時点灯、終わると暗い青色に変化します。環境によりますが1分程度で終了します。

1分以上待ってもキャリブレーションが終わらない場合は以下の条件をご確認ください。かなりシビアな条件です。

- ESP8266実装面が上になるようにボードを置いているか(MPU-6050側ではないことに注意！)
- ボードの水平がとられているか
- 近くにPC,扇風機,その他振動源が存在しないか
- USBシリアルケーブル経由で振動が伝わっていないか

## 動作確認(UDP送信)

まずボードにWiFi APのSSID,パスフレーズ,PCのIPアドレスとUDPポートを教えなければなりません。
この設定はIMUduino6ではサーバモードに入ることで行います。

最終的なトポロジーは以下の通りですが...

 7IMUduino --無線-- WiFi AP --無線--- PC

サーバモードではボード自体がWiFi APになるので以下のような接続になります。

 7IMUduino(AP) --無線-- PC

サーバモードに入るには7IMUduinoのRSTボタンを押して0.5秒待ってIO0ボタンを押し続けてください。
LEDが強力に白色発行するので判別できるはずです。

このとき、PCの無線サービスに7IMUduinoのMACアドレスをSSIDとした仮APが現れるので接続してください。
パスフレーズは「esp8266pass」です。
PCのWebブラウザをひらき、http://192.168.4.1/にアクセスすると各種設定ができます。
コンテンツ内のSendボタンを押して「WiFi AP Accepted」と出れば書きこまれています。

ボードをRSTボタンで再起動し、クライアントモードで起動すると、1秒程度経ってLEDが白色点滅し始めます。
点滅2,3回で終了すれば接続しています。10回以上点滅する場合はつながっていませんので設定を確認してください。

クライアントモードでは有機ELに「温度,気圧,湿度」「Press IO0 Btn.」「QRコード」が順繰りに表示されているはずです。
おもむろにIO0ボタンを1秒以上押すとUDP送信が開始されます。問題なく送信できている場合はLEDは緑色に点灯します。
MPU-6050の初期化に失敗するなどの問題があると赤色に点灯します。ESP8266の処理が間に合わなくてFIFOがオーバフローすると
黄色に点灯します。

問題なければ、PCでUDPTeapot.pdeを起動しているとコンソールに傾きや温度などが表示されるはずです。



