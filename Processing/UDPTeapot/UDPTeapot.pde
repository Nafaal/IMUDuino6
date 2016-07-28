
// I2C device class (I2Cdev) demonstration Processing sketch for MPU6050 DMP output
// 6/20/2012 by Jeff Rowberg <jeff@rowberg.net>
// Updates should (hopefully) always be available at https://github.com/jrowberg/i2cdevlib
//
// Changelog:
//     2012-06-20 - initial release
//     2016-06-14 - UDP version
/* ============================================
 I2Cdev device library code is placed under the MIT license
 Copyright (c) 2012 Jeff Rowberg
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ===============================================
 */

import processing.opengl.*;
import toxi.geom.*;
import toxi.processing.*;
import hypermedia.net.*;

// NOTE: requires ToxicLibs to be installed in order to run properly.
// 1. Download from http://toxiclibs.org/downloads
// 2. Extract into [userdir]/Processing/libraries
//    (location may be different on Mac/Linux)
// 3. Run and bask in awesomeness

ToxiclibsSupport gfx;

final int udp_port = 8870;
UDP udp;

float[] q = new float[4];
Quaternion quat = new Quaternion(1, 0, 0, 0);

float[] gravity = new float[3];
float[] euler = new float[3];
float[] ypr = new float[3];
int[] accel = new int[3];
float[] laccel = new float[3];
float temperture  = 0.0;
float pressure  = 0.0;
float humidity = 0.0;
final int hdroff = 12;

void dispose() {
  println("*exit*");
  udp.close();
}

void settings() {
  // 300px square viewport using OpenGL rendering
  size(300, 300, P3D);
}

void setup() {
  gfx = new ToxiclibsSupport(this);

  // setup lights and antialiasing
  lights();
  smooth();

  udp = new UDP(this, udp_port);
  udp.listen(true);
}

int b2i(byte b)
{
  int tmp = (int)b;
  if (tmp < 0) tmp += 256;
  return tmp;
}

void receive(byte data[]) {
  int seq = (b2i(data[2]) << 8) | b2i(data[3]);
  int t2 = (b2i(data[4]) << 8) | b2i(data[5]);
  int p2 = (b2i(data[6]) << 8) | b2i(data[7]);
  int h2 = (b2i(data[8]) << 8) | b2i(data[9]);
  temperture  = (float)t2 / 10.0;
  pressure  = (float)p2 / 10.0;
  humidity = (float)h2 / 10.0;
  print("temperture:", temperture);
  print(" pressure:", pressure);
  println(" humidity:", humidity);
  // get quaternion from data packet
  q[0] = ((b2i(data[0 + hdroff]) << 8) | b2i(data[1 + hdroff])) / 16384.0f;//w
  q[1] = ((b2i(data[4 + hdroff]) << 8) | b2i(data[5 + hdroff])) / 16384.0f;//x
  q[2] = ((b2i(data[8 + hdroff]) << 8) | b2i(data[9 + hdroff])) / 16384.0f;//y
  q[3] = ((b2i(data[12 + hdroff]) << 8) | b2i(data[13 + hdroff])) / 16384.0f;//z
  for (int i = 0; i < 4; i++) if (q[i] >= 2) q[i] = -4 + q[i];
  // set our toxilibs quaternion to new data
  quat.set(q[0], q[1], q[2], q[3]);

  gravity[0] = 2 * (q[1]*q[3] - q[0]*q[2]);
  gravity[1] = 2 * (q[0]*q[1] + q[2]*q[3]);
  gravity[2] = q[0]*q[0] - q[1]*q[1] - q[2]*q[2] + q[3]*q[3];

  // calculate Euler angles
  euler[0] = atan2(2*q[1]*q[2] - 2*q[0]*q[3], 2*q[0]*q[0] + 2*q[1]*q[1] - 1);
  euler[1] = -asin(2*q[1]*q[3] + 2*q[0]*q[2]);
  euler[2] = atan2(2*q[2]*q[3] - 2*q[0]*q[1], 2*q[0]*q[0] + 2*q[3]*q[3] - 1);

  // calculate yaw/pitch/roll angles
  int sign = 1;
  if (gravity[2] > 0) sign = 1; else sign = -1;
  ypr[0] = atan2(2*q[1]*q[2] - 2*q[0]*q[3], 2*q[0]*q[0] + 2*q[1]*q[1] - 1);
  ypr[1] = atan2(gravity[0], sign * sqrt(0.001*gravity[1]*gravity[1] + gravity[2]*gravity[2]));
  ypr[2] = atan2(gravity[1], sign * sqrt(0.001*gravity[0]*gravity[0] + gravity[2]*gravity[2]));
  print("yaw:", ypr[0] * 180 / 3.14);
  print(" roll:", ypr[1] * 180 / 3.14);
  println(" pitch:", ypr[2] * 180 / 3.14);
    
  // calculate Real Accelometor
  accel[0] = (data[28 + hdroff] << 8) | data[29 + hdroff];
  accel[1] = (data[32 + hdroff] << 8) | data[33 + hdroff];
  accel[2] = (data[36 + hdroff] << 8) | data[37 + hdroff];
  
  // calculate Linear Accelometor
  laccel[0] = accel[0] - gravity[0] * 8102.0f;
  laccel[1] = accel[1] - gravity[1] * 8102.0f;
  laccel[2] = accel[2] - gravity[2] * 8102.0f;  
}

void draw() {
  // black background
  background(0);

  // translate everything to the middle of the viewport
  pushMatrix();
  translate(width / 2, height / 2);

  // 3-step rotation from yaw/pitch/roll angles (gimbal lock!)
  // ...and other weirdness I haven't figured out yet
  //rotateY(-ypr[0]);
  //rotateZ(-ypr[1]);
  //rotateX(-ypr[2]);

  // toxiclibs direct angle/axis rotation from quaternion (NO gimbal lock!)
  // (axis order [1, 3, 2] and inversion [-1, +1, +1] is a consequence of
  // different coordinate system orientation assumptions between Processing
  // and InvenSense DMP)
  float[] axis = quat.toAxisAngle();
  rotate(axis[0], axis[1], -axis[3], axis[2]);

  // draw main body in red
  fill(128, 255, 128, 200);
  box(10, 10, 200);

  // draw front-facing tip in blue
  fill(0, 0, 255, 200);
  pushMatrix();
  translate(0, 0, -120);
  rotateX(PI/2);
  drawCylinder(0, 20, 20, 8);
  popMatrix();

  // draw wings and tail fin in green
  fill(0, 255, 0, 200);
  beginShape(TRIANGLES);
  vertex(-100, 2, 30); 
  vertex(0, 2, -80); 
  vertex(100, 2, 30);  // wing top layer
  vertex(-100, -2, 30); 
  vertex(0, -2, -80); 
  vertex(100, -2, 30);  // wing bottom layer
  vertex(-2, 0, 98); 
  vertex(-2, -30, 98); 
  vertex(-2, 0, 70);  // tail left layer
  vertex( 2, 0, 98); 
  vertex( 2, -30, 98); 
  vertex( 2, 0, 70);  // tail right layer
  endShape();
  beginShape(QUADS);
  vertex(-100, 2, 30); 
  vertex(-100, -2, 30); 
  vertex(  0, -2, -80); 
  vertex(  0, 2, -80);
  vertex( 100, 2, 30); 
  vertex( 100, -2, 30); 
  vertex(  0, -2, -80); 
  vertex(  0, 2, -80);
  vertex(-100, 2, 30); 
  vertex(-100, -2, 30); 
  vertex(100, -2, 30); 
  vertex(100, 2, 30);
  vertex(-2, 0, 98); 
  vertex(2, 0, 98); 
  vertex(2, -30, 98); 
  vertex(-2, -30, 98);
  vertex(-2, 0, 98); 
  vertex(2, 0, 98); 
  vertex(2, 0, 70); 
  vertex(-2, 0, 70);
  vertex(-2, -30, 98); 
  vertex(2, -30, 98); 
  vertex(2, 0, 70); 
  vertex(-2, 0, 70);
  endShape();

  popMatrix();
}

void mousePressed() {
  if( mouseButton == LEFT ) {
    println("*udp close*");
    udp.close();
  }
}
  
void drawCylinder(float topRadius, float bottomRadius, float tall, int sides) {
  float angle = 0;
  float angleIncrement = TWO_PI / sides;
  beginShape(QUAD_STRIP);
  for (int i = 0; i < sides + 1; ++i) {
    vertex(topRadius*cos(angle), 0, topRadius*sin(angle));
    vertex(bottomRadius*cos(angle), tall, bottomRadius*sin(angle));
    angle += angleIncrement;
  }
  endShape();

  // If it is not a cone, draw the circular top cap
  if (topRadius != 0) {
    angle = 0;
    beginShape(TRIANGLE_FAN);

    // Center point
    vertex(0, 0, 0);
    for (int i = 0; i < sides + 1; i++) {
      vertex(topRadius * cos(angle), 0, topRadius * sin(angle));
      angle += angleIncrement;
    }
    endShape();
  }

  // If it is not a cone, draw the circular bottom cap
  if (bottomRadius != 0) {
    angle = 0;
    beginShape(TRIANGLE_FAN);

    // Center point
    vertex(0, tall, 0);
    for (int i = 0; i < sides + 1; i++) {
      vertex(bottomRadius * cos(angle), tall, bottomRadius * sin(angle));
      angle += angleIncrement;
    }
    endShape();
  }
}