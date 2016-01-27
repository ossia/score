// The Flock (a list of Boid objects)
class Flock 
{
  private ArrayList<Boid> boids;     // An ArrayList for all existing boids
  private ArrayList<Boid> new_boids; // An ArrayList for all boids to add the next time
  private boolean new_boids_locked;
  private boolean clear_request;
  
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

  void run() 
  { 
    // debug
    if (new_boids.size() > 0)
    {
      print(new_boids.size());
      println(" boid(s) to add");
    }
    
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
    
    processDeviation();
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
    text("size :", 10, 315);
    text(boids.size(), 50, 315);
    
    text("deviation :", 10, 330);
    text(deviation.x, 85, 330);
    text(deviation.y, 85, 345);
  }

  void addBoid(Boid b) 
  {
    if (!new_boids_locked)
      new_boids.add(b);
    else
      println("new_boids_locked");
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
  
  // note : deviation is meaningless as boids are mirrored when crossing a border
  // but we can know when all boids are together if deviation is small
  PVector getDeviation()
  {
    return deviation;
  }
 
  private void processDeviation()
  {
    deviation = new PVector(0, 0);
    
    if (boids.size() > 0)
    {
      // process mean
      PVector mean = new PVector(0, 0);
      for (Boid b : boids) 
      {
        mean.add(b.location);
      }
      mean.div(boids.size());

      // process deviation
      for (Boid b : boids) 
      {
        float x_xm2 = pow(b.location.x - mean.x, 2);
        float y_ym2 = pow(b.location.y - mean.y, 2);

        deviation.add(new PVector(x_xm2, y_ym2));
      }
      deviation.div(boids.size());
      deviation = new PVector(sqrt(deviation.x), sqrt(deviation.y));
    }
  }
}

