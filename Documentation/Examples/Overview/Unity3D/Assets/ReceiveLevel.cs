using UnityEngine;
using System.Collections;

public class ReceiveLevel : MonoBehaviour {

	public OSC oscReference;

	void Start () {
		oscReference.SetAddressHandler( "/my_pos" , OnPos );
		oscReference.SetAddressHandler( "/my_float" , OnFloat );
	}

	void OnPos(OscMessage message) {
		float x = message.GetFloat(0);
		float y = message.GetFloat(1);

		this.GetComponent<Rigidbody>().velocity = 
			new Vector3(x, y, transform.position.z);
	}

	void OnFloat(OscMessage message) {
		float v = message.GetFloat(0) / 20.0f;
		transform.localScale = new Vector3(v,v,v);
	}

	void Update () {
	
	}
}
	