# IMUDuino6

## 概要

ESP8366,3軸ジャイロセンサー,3軸加速度センサー,気象センサー,フルカラーLED,有機ELディスプレイを統合した
[7IMUduino](http://www.papa.to/kinowiki/index.php/Products/7IMUduino) のためのファームウェアです。
全ての機能をテストするために盛りだくさんとなっております。

ESP8266モジュールを組み込んだArduino IDE(1.6.9)でビルドが可能です。

## フィーチャー

- 初回起動時のキャリブレーションモードではジャイロセンサーの誤差を自動修正します
- クライアントモードではセンサーデータをUDPパケットにして所定のPCへ送信します
- サーバモードではWebブラウザからSSID,センサーオフセット,サーバアドレスを設定できます
- 有機ELディスプレイにBME280から得た温度, 気圧, 湿度を表示します
- 有機ELディスプレイにMPU6050から得た傾き(Yaw,Roll,Pitch)を表示します
- 有機ELディスプレイにADCから得た0〜5Vまでの電圧(mV)を表示します
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

これらのライブラリはライブラリマネージャからインストールしてもかまいませんが、
このソースにはBME280ライブラリのWire初期化をバイパスしたり、MPU-6050のYawRollPich取得関数を改善するパッチが含まれています。
パッチの当て方はlibinstall.shを参考にしてください。

なお、ライブラリはデフォルトでは各ホームディレクトリのArduino/librariesディレクトリに置きます。

>+libraries  
>　　+I2CDev  
>　　+MPU6050  
>　　...  

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

なお、ライブラリはデフォルトでは各ホームディレクトリのProcessing/librariesディレクトリに置きます。
toxiclibs-complete-20.zipを展開したディレクトリ、udp.zipを展開したディレクトリをそのままlibrariesにコピーしてください。

> +libraries  
>　　+audioutils  
>　　...  
>　　+udp  
>　　+verletphysics  
>　　+volumeutils  

UDPTeapot.pdeをダブルクリックすればProcessingが起動します。


## 動作確認(キャリブレーション)

ボードの初回起動時は自動的にキャリブレーションモードになります。LEDはパープルに常時点灯しますので判別できるはずです。
有機ELには「Calibration...」と表示されます。

この状態で、可能限りボードが水平になるように設置して、静かにIO0ボタンを押すと「Start...」と表示されてキャリブレーションが
開始されます。キャリブレーション中はLEDは水色に常時点灯、終わると暗い青色に変化します。環境によりますが1分程度で終了します。

1分以上待ってもキャリブレーションが終わらない場合は以下の条件をご確認ください。かなりシビアな条件です。

- ESP8266実装面が上になるようにボードを置いているか(MPU-6050側ではないことに注意！)
- ボードの水平がとられているか
- 近くにPC,扇風機,その他振動源が存在しないか
- USBシリアルケーブル経由で振動が伝わっていないか

あまりに安定しない場合はリチウムポリマー電池の利用も検討してください。


## 動作確認(UDP送信)

まずボードにWiFi APのSSID,パスフレーズ,PCのIPアドレスとUDPポートを書きこまねばなりません。
この設定はIMUduino6ではサーバモードに入ることで行います。

最終的なトポロジーは以下の通りですが...

 7IMUduino --無線-- ご家庭のWiFi AP --無線--- PC

サーバモードではボード自体がWiFi APになるので以下のような接続になります。

 7IMUduino(仮AP) --無線-- PC

サーバモードに入るには7IMUduinoのRSTボタンを押して0.5秒待ってIO0ボタンを押し続けてください。
LEDが強力に白色発光するので判別できるはずです。

このとき、PCの無線サービスに7IMUduinoのMACアドレスをSSIDとした仮APが現れるので接続してください。
パスフレーズは「esp8266pass」です。
PCのWebブラウザをひらき、 http://192.168.4.1/ にアクセスすると各種設定ができます。
コンテンツ内のSendボタンを押して書き込みます。「WiFi AP Accepted」と出れば書きこまれています。

RSTボタンで再起動し、ボードがクライアントモードで起動すると、1秒程度経ってLEDが白色点滅し始めます。
点滅2,3回で終了すればだいたい接続完了しています。10回以上点滅する場合はSSID等の設定を確認してください。

おもむろにIO0ボタンを1秒以上押すとUDP送信が開始されます。LEDによる状態表示は以下の通りです。

- 問題なく送信できている場合は緑色に点灯します
- MPU-6050の初期化に失敗する,WiFiが切断された、などの問題があると赤色に点灯します
- 送信開始後ESP8266の処理が間に合わなくてFIFOがオーバフローすると黄色に点灯します

有機ELディスプレイがある場合、正常に起動したクライアントモードでは以下の画面が順繰りに表示されているはずです。

1. 「温度,気圧,湿度」
2. 「ヨー・ピッチ・ロール」
3. 「ADCの電圧とサイトのQRコード」

ボードを動かしてヨー・ピッチ・ロールが変化していれば大体大丈夫だと思います。

このとき、PCでUDPTeapot.pdeを起動していると、Processingのコンソールに、上記の情報がダンプされているはずです。

## ボード設定のリセット

設定を間違った時や、キャリブレーションからやり直したい場合は、クライアントモードで20秒ほどIO0ボタンを長押しすると
LEDが紫色に10回点滅してボード設定が削除されます。ここでRSTボタンでリセットすればキャリブレーションから始まるはずです。

