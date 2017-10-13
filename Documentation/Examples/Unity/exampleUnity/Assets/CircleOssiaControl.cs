using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class CircleOssiaControl : MonoBehaviour {

	[Ossia.Expose("pos")]
	public Vector3 pos;

	[Ossia.Expose("rot")]
	public Vector3 rot;

	[Ossia.Expose("color")]
	public Color color;

	// Use this for initialization
	void Start () {
		
	}
	
	// Update is called once per frame
	void Update () {
		transform.position = pos;
		transform.eulerAngles = rot;

		var myMaterial = GetComponent<Renderer>().material;
		myMaterial.color = color;
	}
}
