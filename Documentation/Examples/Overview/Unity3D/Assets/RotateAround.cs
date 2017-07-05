using UnityEngine;
using System.Collections;

public class RotateAround : MonoBehaviour {

	public OSC oscReference;

	void Start () {
		oscReference.SetAddressHandler( "/my_int" , OnInt );
		oscReference.SetAddressHandler( "/my_color" , OnColor );
	}


	void OnInt(OscMessage message) {
		int v = message.GetInt(0);
		var ps = this.GetComponent<ParticleSystem> ().main;
		ps.maxParticles = v;
	}

	void OnColor(OscMessage message) {
		float r = message.GetFloat(0);
		float g = message.GetFloat(1);
		float b = message.GetFloat(2);
		var ps = this.GetComponent<ParticleSystem> ().main;
	    ps.startColor = new Color (r, g, b);

	}

	void Update () {

	}
}
