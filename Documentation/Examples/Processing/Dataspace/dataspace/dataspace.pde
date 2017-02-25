// NOTE : forked from  https://www.openprocessing.org/sketch/147268
import oscP5.*;
import netP5.*;
import java.util.Random;

OscP5 osc_in;
NetAddress osc_out;

final int NB_PARTICLES = 200;
ArrayList<Triangle> triangles;
Particle[] parts = new Particle[NB_PARTICLES];
PImage image;
MyColor myColor = new MyColor();

void setup()
{
  size(1024, 768, P2D);
  
  // listen osc message from i-score
  osc_in = new OscP5(this, 13001);

  // output osc messages to i-score
  osc_out = new NetAddress("127.0.0.1", 13002);
  
  triangles = new ArrayList<Triangle>();
  for (int i = 0; i < NB_PARTICLES; i++)
  {
    parts[i] = new Particle();
  }
}

Random rand = new Random();
void dumpOSC()
{
  {
  OscMessage osc_msg = new OscMessage("/mouse/click");
  osc_msg.add(mouseX);
  osc_msg.add(mouseY);
  osc_in.send(osc_msg, osc_out);
  }
  {
  OscMessage osc_msg = new OscMessage("/mouse/move");
  osc_msg.add(mouseX);
  osc_msg.add(mouseY);
  osc_msg.add(rand.nextFloat() * 100 - 50);
  osc_in.send(osc_msg, osc_out);
  }
  {
  OscMessage osc_msg = new OscMessage("/mouse/release");
  osc_msg.add(mouseX);
  osc_msg.add(mouseY);
  osc_in.send(osc_msg, osc_out);
  }
  {
  OscMessage osc_msg = new OscMessage("/particle/density");
  osc_msg.add(0.f);
  osc_in.send(osc_msg, osc_out);
  }
  
  {
  OscMessage osc_msg = new OscMessage("/particle/radius");
  osc_msg.add(0.f);
  osc_in.send(osc_msg, osc_out);
  }
  {
  OscMessage osc_msg = new OscMessage("/color");
  osc_msg.add(0.f);
  osc_msg.add(0.f);
  osc_msg.add(0.f);
  osc_in.send(osc_msg, osc_out);
  }
  
}

void oscEvent(OscMessage osc_msg)
{
  if (osc_msg.checkAddrPattern("/particle/density")) 
  {
    if (osc_msg.checkTypetag("f")) 
    { 
      for (int i = 0; i < NB_PARTICLES; i++)
      {
        parts[i].DIST_MAX = osc_msg.get(0).floatValue();
      }
    }
  }
  else if (osc_msg.checkAddrPattern("/color")) 
  {
    if (osc_msg.checkTypetag("fff")) 
    { 
        myColor.R = osc_msg.get(0).floatValue() * 255;
        myColor.G = osc_msg.get(1).floatValue() * 255;
        myColor.B = osc_msg.get(2).floatValue() * 255;
    }
  }
  else if(osc_msg.checkAddrPattern("/particle/radius"))
  {
    if (osc_msg.checkTypetag("f")) 
    { 
      for (int i = 0; i < NB_PARTICLES; i++)
      {
        parts[i].RAD = osc_msg.get(0).floatValue();
      }
    }
  }
}
void draw()
{
  noStroke();
  fill(120, 1);
  background(50);
  triangles.clear();
  Particle p1, p2;

  for (int i = 0; i < NB_PARTICLES; i++)
  {
    parts[i].move();
    parts[i].display();
  }

  for (int i = 0; i < NB_PARTICLES; i++)
  {
    p1 = parts[i];
    p1.neighboors.clear();
    p1.neighboors.add(p1);
    for (int j = i+1; j < NB_PARTICLES; j++)
    {
      p2 = parts[j];
      float d = PVector.dist(p1.pos, p2.pos); 
      if (d > 0 && d < p2.DIST_MAX)
      {
        p1.neighboors.add(p2);
      }
    }
    if(p1.neighboors.size() > 1)
    {
      addTriangles(p1.neighboors);
    }
  }
  drawTriangles();
}

void drawTriangles()
{
  noStroke();
  fill(myColor.R, myColor.G, myColor.B, myColor.A);
  stroke(max(myColor.R-15, 0), max(myColor.G-15, 0), max(myColor.B-15, 0), 13);
  //noFill();
  beginShape(TRIANGLES);
  for (int i = 0; i < triangles.size(); i ++)
  {
    Triangle t = triangles.get(i);
    t.display();
  }
  endShape();  
}

void addTriangles(ArrayList<Particle> p_neighboors)
{
  int s = p_neighboors.size();
  if (s > 2)
  {
    for (int i = 1; i < s-1; i ++)
    { 
      for (int j = i+1; j < s; j ++)
      { 
         triangles.add(new Triangle(p_neighboors.get(0).pos, p_neighboors.get(i).pos, p_neighboors.get(j).pos));
      }
    }
  }
}

void mousePressed()
{
  if(mouseY < 50) dumpOSC();
  OscMessage osc_msg = new OscMessage("/mouse/click");
  osc_msg.add((float)mouseX);
  osc_msg.add((float)mouseY);
  osc_in.send(osc_msg, osc_out);
  println("### " + osc_msg.addrPattern());
}

void mouseMoved()
{
  OscMessage osc_msg = new OscMessage("/mouse/move");
  osc_msg.add((float)mouseX);
  osc_msg.add((float)mouseY);
  osc_in.send(osc_msg, osc_out);
}
void mouseReleased()
{
  OscMessage osc_msg = new OscMessage("/mouse/release");
  osc_msg.add((float)mouseX);
  osc_msg.add((float)mouseY);
  osc_in.send(osc_msg, osc_out);
}



class MyColor
{
  float R, G, B, A;
  final static float minSpeed = .7;
  final static float maxSpeed = 1.5;
  MyColor()
  {
    R = random(255);
    G = random(255);
    B = random(255);
    A = 100;
  }
}

class Particle
{
  float RAD = 20;
  float BOUNCE = -1;
  float SPEED_MAX = 2.2;
  float DIST_MAX = 50;
  PVector speed = new PVector(random(-SPEED_MAX, SPEED_MAX), random(-SPEED_MAX, SPEED_MAX));
  PVector acc = new PVector(0, 0);
  PVector pos;
  //neighboors contains the particles within DIST_MAX distance, as well as itself
  ArrayList<Particle> neighboors;
  
  Particle()
  {
    pos = new PVector (random(width), random(height));
    neighboors = new ArrayList<Particle>();
  }

  public void move()
  {    
    pos.add(speed);
    
    acc.mult(0);
    
    if (pos.x < 0)
    {
      pos.x = 0;
      speed.x *= BOUNCE;
    }
    else if (pos.x > width)
    {
      pos.x = width;
      speed.x *= BOUNCE;
    }
    if (pos.y < 0)
    {
      pos.y = 0;
      speed.y *= BOUNCE;
    }
    else if (pos.y > height)
    {
      pos.y = height;
      speed.y *= BOUNCE;
    }
  }
  
  public void display()
  {
    fill(255, 14);
    ellipse(pos.x, pos.y, RAD, RAD);
  }
}

class Triangle
{
  PVector A, B, C; 

  Triangle(PVector p1, PVector p2, PVector p3)
  {
    A = p1;
    B = p2;
    C = p3;
  }
  
  public void display()
  {
    vertex(A.x, A.y);
    vertex(B.x, B.y);
    vertex(C.x, C.y);
  }
}
