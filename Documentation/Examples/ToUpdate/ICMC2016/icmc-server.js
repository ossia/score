var ws = require("nodejs-websocket")

Math.clip = function(number, min, max) {
  return Math.max(min, Math.min(number, max));
}

function getRandomColor() {
    var letters = '0123456789ABCDEF';
    var color = '#';
    for (var i = 0; i < 6; i++ ) {
        color += letters[Math.floor(Math.random() * 16)];
    }
    return color;
}

function getRandomName() {
    var letters = 'aeioubcdfmnprstwxyz';
    var name = '';
    for (var i = 0; i < 6; i++ ) {
        name += letters[Math.floor(Math.random() * 16)];
    }
    return name;
}
function tryParseJSON (jsonString){
    try {
        var o = JSON.parse(jsonString);
        if (o && typeof o === "object") {
            return o;
        }
    }
    catch (e) { }
    return false;
};

function mode(arr){
    return arr.sort((a,b) =>
          arr.filter(v => v===a).length
        - arr.filter(v => v===b).length
    ).pop();
}


function Client(name, color, connection) {
    this.name = name;
    this.color = color;
    this.connection = connection;
    this.speed = 1.;
    this.next = 1;
}

// A list of connection with two parameters :
// Current constraint speed, and next condition vote
var clients = new Array;

// The i-score client
var iscore_client;

var iscore_receiver = ws.createServer(
function (conn)
{
    console.log("i-score connected");
    conn.on("close", function (code, reason) {
        console.log("Iscore Connection closed");
        iscore_client = null;
    })

    conn.on('error', function() {
        console.log('i-score ERROR');
        iscore_client = null;
    });

    iscore_client = conn;
}).listen(8001)


function updateClient(value, id)
{
    try{

    console.log("Parsed: " + value.speed + " " + value.next);
    // Update the value in the client
    var sumSpeed = 0;
    for(var i = 0; i < clients.length; i++)
    {
        if(clients[i].connection === id)
        {
            clients[i].speed = value.speed;
            clients[i].next = value.next;
        }
        console.log(clients[i].speed);
        sumSpeed += clients[i].speed;
    }

    // Compute the current values for speed / vote and send it to the server
    var meanSpeed = sumSpeed / clients.length;
    var maxVote = Math.clip(Number(mode(clients.map(a => a.next))) + 1, 1, 6);

    try {
    if (iscore_client.readyState == iscore_client.OPEN) {
        iscore_client.sendText(JSON.stringify({ speed: Number(meanSpeed), next: Number(maxVote) }));
    }
    } catch(e) { }

    } catch(e) {
        console.log("error in updateClient");
    }
}

var clients_receiver = ws.createServer(
function (conn)
{
    console.log("Connected");
    conn.on("text", function (str)
    {
        console.log("Received "+str)
        var res = tryParseJSON(str);
        if(res)
        {
            updateClient(res, conn);
        }
    })

    conn.on("close", function (code, reason) {
        console.log("Connection closed")
        clients = clients.filter(e => e.connection !== conn);
    })

    conn.on('error', function() {
        console.log('client ERROR');
        clients = clients.filter(e => e.connection !== conn);
    });


    var client = new Client(getRandomName(), getRandomColor(), conn);
    if (client.connection.readyState == client.connection.OPEN) {
        client.connection.sendText(JSON.stringify({ name: client.name , color: client.color }));
        clients.push(client)
    }

}).listen(8002)


