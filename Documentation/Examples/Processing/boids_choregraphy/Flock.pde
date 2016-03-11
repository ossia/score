// The Flock (a list of Boid objects)
class Flock 
{
  private ArrayList<Boid> boids;     // An ArrayList for all existing boids
  private ArrayList<Boid> new_boids; // An ArrayList for all boids to add the next time
  private boolean new_boids_locked;
  private boolean clear_request;

  private PVector position;
  private PVector direction;
  private PVector deviation;

  Flock() 
  {
    boids = new ArrayList<Boid>();
    new_boids = new ArrayList<Boid>();
    new_boids_locked = false;
    clear_request = false;
    deviation = new PVector(0, 0);
  }

  void clear() 
  {
    clear_request = true;
  }

  int run() 
  { 
    int size_before = getSize();
    
    // handle clear request before new boids
    if (clear_request)
    {
      boids.clear();
      clear_request = false;
    }

    // add all new boids
    new_boids_locked = true;
    for (Boid b : new_boids) 
    {
      boids.add(b);
    }
    new_boids.clear();
    new_boids_locked = false;

    // update all existing boids
    for (Boid b : boids) 
    {
      b.run(boids);
    }

    processPositionDirectionDeviation();
    
    return getSize() - size_before;
  }

  void draw_info()
  {
    // display follow_destination as a circle if follow_rate > 0.
    float follow_rate = getFollowRate();

    if (follow_rate > 0.0)
    {
      PVector follow_destination = getFollowDestination();

      ellipseMode(CENTER);
      fill(200, 100, 100);
      noStroke();
      ellipse(follow_destination.x, follow_destination.y, follow_rate*10, follow_rate*10);
    }

    // display flock parameters as text
    fill(200);
    text("avoidance :", 10, 15);
    text(getAvoidance(), 85, 15);

    text("alignement :", 10, 30);
    text(getAlignment(), 85, 30);

    text("cohesion :", 10, 45);
    text(getCohesion(), 85, 45);

    text("dist min :", 10, 60);
    text(getDistanceMin(), 85, 60);

    text("dist max :", 10, 75);
    text(getDistanceMax(), 85, 75);

    // display flock statistics
    text("size :", 160, 15);
    text(getSize(), 200, 15);

    stroke(200, 100, 100);
    line(position.x, position.y, position.x + direction.x*10., position.y + direction.y*10.);

    text("deviation :", 160, 30);
    text(deviation.x, 235, 30);
    text(deviation.y, 235, 45);
  }

  void addBoid(Boid b) 
  {
    if (!new_boids_locked)
      new_boids.add(b);
    else
      println("new_boids_locked");
  }

  void setKill(boolean kill) 
  {
    for (Boid b : boids) 
    {
      b.kill_out = kill;
    }
  }

  void setAvoidance(float avoidance) 
  {
    for (Boid b : boids) 
    {
      b.avoidance = avoidance;
    }
  }

  float getAvoidance()
  {
    if (boids.size() > 0)
    {
      return boids.get(0).avoidance;
    } else
    {
      return 1.0;
    }
  }

  void setAlignment(float alignment) 
  {
    for (Boid b : boids) 
    {
      b.alignment = alignment;
    }
  }

  float getAlignment()
  {
    if (boids.size() > 0)
    {
      return boids.get(0).alignment;
    } else
    {
      return 1.0;
    }
  }

  void setCohesion(float cohesion) 
  {
    for (Boid b : boids) 
    {
      b.cohesion = cohesion;
    }
  }

  float getCohesion()
  {
    if (boids.size() > 0)
    {
      return boids.get(0).cohesion;
    } else
    {
      return 1.0;
    }
  }

  void setDistanceMin(float distance_min) 
  {
    for (Boid b : boids) 
    {
      b.neighbor_distance_min = distance_min;
    }
  }

  float getDistanceMin()
  {
    if (boids.size() > 0)
    {
      return boids.get(0).neighbor_distance_min;
    } else
    {
      return 50.0;
    }
  }

  void setDistanceMax(float distance_max) 
  {
    for (Boid b : boids) 
    {
      b.neighbor_distance_max = distance_max;
    }
  }

  float getDistanceMax()
  {
    if (boids.size() > 0)
    {
      return boids.get(0).neighbor_distance_max;
    } else
    {
      return 75.0;
    }
  }

  void setFollowDestination(int x, int y)
  {
    for (Boid b : boids) 
    {
      b.follow_destination = new PVector(x, y);
    }
  }

  PVector getFollowDestination()
  {
    if (boids.size() > 0)
    {
      return boids.get(0).follow_destination;
    } else
    {
      return new PVector(0, 0);
    }
  }

  void setFollowRate(float follow_rate)
  {
    for (Boid b : boids) 
    {
      b.follow_rate = follow_rate;
    }
  }

  float getFollowRate()
  {
    if (boids.size() > 0)
    {
      return boids.get(0).follow_rate;
    } else
    {
      return 0.0;
    }
  }

  int getSize()
  {
    int count = 0;
    for (Boid b : boids) 
    {
      if (!b.alive)
        continue;
      count++;
    }

    return count;
  }
  
  boolean getCollision()
  {
    for (Boid b : boids) 
    {
      if (b.collision)
        return true;
    }

    return false;
  }

  PVector getPosition()
  {
    return position;
  }

  PVector getDirection()
  {
    return direction;
  }

  // note : deviation is meaningless as boids are mirrored when crossing a border
  // but we can know when all boids are together if deviation is small
  PVector getDeviation()
  {
    return deviation;
  }

  private void processPositionDirectionDeviation()
  {
    position = new PVector(0, 0);
    direction = new PVector(0., 0.);
    deviation = new PVector(0, 0);

    if (boids.size() > 0)
    {
      // process means
      int count = 0;
      for (Boid b : boids) 
      {
        if (!b.alive)
          continue;

        count++;
        position.add(b.location);
        direction.add(b.velocity);
      }
      position.div(count);
      direction.div(count);

      // process deviation
      for (Boid b : boids) 
      {
        float x_xm2 = pow(b.location.x - position.x, 2);
        float y_ym2 = pow(b.location.y - position.y, 2);

        deviation.add(new PVector(x_xm2, y_ym2));
      }
      deviation.div(count);
      deviation = new PVector(sqrt(deviation.x), sqrt(deviation.y));
    }
  }
}

