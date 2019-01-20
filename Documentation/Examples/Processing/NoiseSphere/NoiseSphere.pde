/**
 * Based on Noise Sphere by David Pena.  
 */

// To use :
// * Download http://repo1.maven.org/maven2/net/java/dev/jna/jna/5.2.0/jna-5.2.0.jar
//   - rename it to jna.jar
//   - put it in a "code" folder in the sketchbook
//   e.g. ~/sketchbook/NoiseSphere/code/jna.jar
// * Then do the same with the ossia-java release for your operating system:
//   e.g. ~/sketchbook/NoiseSphere/code/ossia-java.jar


import io.ossia.*;
color getColor(Parameter p)
{
  final String s = p.getUnit();
  switch(s) {
    case "color.rgb":
    {
      Vec3F v = p.getVec3();
      return color(v.x * 255., v.y * 255., v.z * 255.);
    }
    case "color.rgba":
    {
      Vec4F v = p.getVec4();
      return color(v.x * 255., v.y * 255., v.z * 255., v.w * 255.);
    }
    case "color.rgb8":
    {
      Vec3F v = p.getVec3();
      return color(v.x, v.y, v.z);
    }
    case "color.rgba8":
    {
      Vec4F v = p.getVec4();
      return color(v.x, v.y, v.z, v.w);
    }
    case "color.hsv":
    {
      Vec3F v = p.getVec3();
      return color(v.x, v.y, v.z);
    }
    case "color.hsl":
    {
      Vec3F v = p.getVec3();
      return color(v.x, v.y, v.z);
    }
    default:
      return color(0);
  }
}



Protocol p = new OscQueryServer(1234, 5678);
Device d = new Device(p, "noise_sphere");
Node root = d.getRootNode();

Parameter c;
Parameter p1;
Parameter p2;
Parameter weight;
Parameter ratio;


int cuantos = 4000;
Pelo[] lista ;
float[] z = new float[cuantos]; 
float[] phi = new float[cuantos]; 
float[] largos = new float[cuantos]; 
float rx = 0;
float ry =0;

void setup() {
  size(640, 360, P3D);
  lista = new Pelo[cuantos];
  for (int i=0; i<cuantos; i++) {
    lista[i] = new Pelo();
  }
  noiseDetail(5);
  
  c = root.create("/color", "color.rgba8");
  
  p1 = root.create("/p1", "float");
  p1.push(200);
  
  p2 = root.create("/p2", "float");
  p2.push(150);
  
  weight = root.create("/weight", "float");
  weight.push(10);
  
  ratio = root.create("/ratio", "float");
  ratio.push(100);
}

void draw() {
  background(0);
  translate(width/2, height/2);

  float rxp = ((mouseX-(width/2))*0.005);
  float ryp = ((mouseY-(height/2))*0.005);
  rx = (rx*0.9)+(rxp*0.1);
  ry = (ry*0.9)+(ryp*0.1);
  rotateY(rx);
  rotateX(ry);
  fill(0);
  noStroke();
  sphere(ratio.getFloat());
  for (int i = 0;i < cuantos; i++) {
    lista[i].draw();
  }
}


class Pelo {
  Parameter ratio = root.create("/pelos/pelo.0/ratio", "float");
  float phi = random(TWO_PI);
  float largo = random(1.15, 1.2);
  float z;

  Pelo() {
    ratio.push(random(100, 300));
    z = random(-ratio.getFloat(), ratio.getFloat());
  }
  
  void draw() {
    float ratio = this.ratio.getFloat();
    float theta = asin(z/ratio);
    float off = (noise(millis() * 0.0005, sin(phi))-0.5) * 0.3;
    float offb = (noise(millis() * 0.0007, sin(z) * 0.01)-0.5) * 0.3;

    float thetaff = theta+off;
    float phff = phi+offb;
    float x = ratio * cos(theta) * cos(phi);
    float y = ratio * cos(theta) * sin(phi);
          z = ratio * sin(theta);

    float xo = ratio * cos(thetaff) * cos(phff);
    float yo = ratio * cos(thetaff) * sin(phff);
    float zo = ratio * sin(thetaff);

    float xb = xo * largo;
    float yb = yo * largo;
    float zb = zo * largo;

    beginShape(LINES);
    stroke(getColor(c));
    strokeWeight(weight.getFloat());
    vertex(x, y, z);
    stroke(p1.getFloat(), p2.getFloat());
    vertex(xb, yb, zb);
    endShape();
  }
}
