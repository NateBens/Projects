var addButton = document.querySelector("#addGame")
addButton.onclick = function () {
    var namefield = document.querySelector("#name")
    var name = namefield.value
    var genrefield = document.querySelector("#genre")
    var genre = genrefield.value
    var sizefield = document.querySelector("#size")
    var size = sizefield.value
    var datefield = document.querySelector("#date")
    var date = datefield.value
    var pubfield = document.querySelector("#publisher")
    var publisher = pubfield.value
    namefield.value = ("");
    genrefield.value = ("");
    sizefield.value = ("");
    datefield.value = ("");
    pubfield.value = ("");
    createGame(name, genre, size, date, publisher);
    // getLastGame();
};

var logoutButton = document.querySelector("#logout")
logoutButton.onclick = function () {
    console.log("logout clicked")
    fetch("https://nate-bens-vgdb.herokuapp.com/sessions", {
        method: "DELETE",
        credentials: 'include',
    }).then(function () {
        console.log("Succefully Logged out.");
        loadGames();
        });
    
};


var createGame = function (name, genre, size, date, publisher) {
    var data = `name=${encodeURIComponent(name)}`;
    data += `&genre=${encodeURIComponent(genre)}`;
    data += `&size=${encodeURIComponent(size)}`;
    data += `&date=${encodeURIComponent(date)}`;
    data += `&publisher=${encodeURIComponent(publisher)}`;
    fetch("https://nate-bens-vgdb.herokuapp.com/videogames", {
        method: "POST",
        body: data,
        credentials: 'include',
        headers: {
            "content-type": " application/x-www-form-urlencoded"
        }
    }).then(function (response) {
        console.log(name, "Succesfully posted");
        loadGames();
    });
};

var showGames = function (games) {
    var loginDiv = document.querySelector("#login");
    loginDiv.style.display = "none";
    var body = document.querySelector("#body");
    body.style.display = "block";
    videogameslist = games;
    var list = document.querySelector("#list");
    list.innerHTML = "";

    games.forEach(function (game) {
        var listItem = document.createElement("li");
        listItem.className = ("videogame")
        listItem.id = (game.name)
        listItem.onclick = gameOnClick;
        listItem.innerHTML = game.name;
        list.appendChild(listItem);
    });
    fetch("https://nate-bens-vgdb.herokuapp.com/users", {
        credentials: 'include',
    }).then(function (response) {
        console.log(response.status)
        if (response.status == 401) {
            loadGames();
        }
        else if (response.status == 200) {
            response.json().then(function (user) {
                //console.log("User:", user);
                var username = user.username;
                //console.log(username);
                var welcomeHeader = document.querySelector("#welcomeheader");
                welcomeHeader.innerHTML = ("Welcome " + username + "!")
            });
        }
        });
    

};

var gameOnClick = function (game_name) {
    var game_id = game_name.path[0].id;

    fetch("https://nate-bens-vgdb.herokuapp.com/videogames", {
        credentials: 'include'
    }).then(function (response) {
        response.json().then(function (theGames) {
            for (var i = 0; i < theGames.length; i++) {
                if (theGames[i].name == game_id) {
                    var selectedGame = theGames[i];
                }
            }
            displayGameInfo(selectedGame);
        });
    });
};

var displayGameInfo = function (game) {
    var bottom = document.querySelector("#bottom");
    bottom.innerHTML = "";

    var name = document.createElement("h3");
    name.className = ("bottomInfo");
    name.id = ("nameDisplay");

    name.innerHTML = ("NAME: " + game.name);
    bottom.appendChild(name);

    var genre = document.createElement("h3");
    genre.id = ("genreDisplay");
    genre.className = ("bottomInfo");
    genre.innerHTML = ("GENRE: " + game.genre);
    bottom.appendChild(genre);

    var size = document.createElement("h3");
    size.id = ("sizeDisplay");
    size.className = ("bottomInfo");
    size.innerHTML = ("SIZE: " + game.size);
    bottom.appendChild(size);

    var date = document.createElement("h3");
    date.id = ("dateDisplay");
    date.className = ("bottomInfo");
    date.innerHTML = ("DATE: " + game.date);
    bottom.appendChild(date);

    var publisher = document.createElement("h3");
    publisher.id = ("publisherDisplay");
    publisher.className = ("bottomInfo");
    publisher.innerHTML = ("PUBLISHER: " + game.publisher);
    bottom.appendChild(publisher);

    var deleteButton = document.querySelector("#delete");
    deleteButton.style.display = "inline";
    var editButton = document.querySelector("#edit");
    editButton.style.display = "inline";
    deleteButton.onclick = function () {
        {
            console.log("Deleting:", game.name);
            fetch(`https://nate-bens-vgdb.herokuapp.com/videogames/${game.id}`, {
                method: "DELETE",
                credentials: "include"
            }).then(function () {
                loadGames();
                console.log("Delete game successful.");
            });
            bottom.innerHTML = ("");

            deleteButton.style.display = "none";
            editButton.style.display = "none";
        }
    };
    editButton.onclick = function () {
        {
            console.log("Editing:", game.name);
            var nameField = document.querySelector("#name");
            nameField.value = (game.name);
            var genreField = document.querySelector("#genre");
            genreField.value = (game.genre);
            var sizeField = document.querySelector("#size");
            sizeField.value = (game.size);
            var dateField = document.querySelector("#date");
            dateField.value = (game.date);
            var publisherField = document.querySelector("#publisher");
            publisherField.value = (game.publisher);
            var updateButton = document.querySelector("#addGame");
            updateButton.innerHTML = ('<span id="button-span">UPDATE</span>');
            updateButton.onclick = function () {
                var name = nameField.value;
                var genre = genreField.value;
                var size = sizeField.value;
                var date = dateField.value;
                var publisher = publisherField.value;
                var data = `name=${encodeURIComponent(name)}`;
                data += `&genre=${encodeURIComponent(genre)}`;
                data += `&size=${encodeURIComponent(size)}`;
                data += `&date=${encodeURIComponent(date)}`;
                data += `&publisher=${encodeURIComponent(publisher)}`;
                fetch(`https://nate-bens-vgdb.herokuapp.com/videogames/${game.id}`, {
                    method: "PUT",
                    body: data,
                    credentials: 'include',
                    headers: {
                        "content-type": " application/x-www-form-urlencoded"
                    }
                }).then(function (response) {
                    console.log(game.name, "Succesfully Edited");
                    loadGames();
                });
                updateButton.innerHTML = ('<span id="button-span">ADD GAME</span>');
                nameField.value = ("");
                genreField.value = ("");
                sizeField.value = ("");
                dateField.value = ("");
                publisherField.value = ("");
                bottom.innerHTML = ("");
                updateButton = null;
                addButton = document.querySelector("#addGame");
                deleteButton.style.display = "none";
                editButton.style.display = "none";
                addButton.onclick = function () {
                    var field = document.querySelector("#name")
                    var name = field.value
                    var field = document.querySelector("#genre")
                    var genre = field.value
                    var field = document.querySelector("#size")
                    var size = field.value
                    var field = document.querySelector("#date")
                    var date = field.value
                    var field = document.querySelector("#publisher")
                    var publisher = field.value
                    createGame(name, genre, size, date, publisher);
                    // getLastGame();
                };
            };

        };

    };
};

//NOT USED, FUNCTION INSIDE OF DISPLAYGAMEINFO USED
var deleteGame = function (game) {
    {
        console.log("Deleting, ", game.name);
        fetch(`https://nate-bens-vgdb.herokuapp.com/videogames/${game.id}`, {
            method: "DELETE",
            credentials: 'include',
        }).then(function () {
            console.log("Delete game successful.");

        });
    }
};

var updateLatestGame = function (games) {
    var list = document.querySelector("#list");
    var last = games[games.length - 1];
    var listItem = document.createElement("li");
    listItem.className = ("fade-in")
    listItem.innerHTML = last;
    list.appendChild(listItem);
};

var getLastGame = function () {
    fetch("https://nate-bens-vgdb.herokuapp.com/videogames", {
        credentials: 'include',
    }).then(
        function (response) {
            response.json().then(function (theGames) {
                console.log("TheGames:", theGames);
                updateLatestGame(theGames);
            });
        });
};

var showLogin = function () {
    var loginDiv = document.querySelector("#login");
    loginDiv.style.display = "block";
    var registerDiv = document.querySelector("#registration");
    registerDiv.style.display = "none";
    var loginButton = document.querySelector("#loginButton");
    var registerLink = document.querySelector("#registrationLink")
    registerLink.onclick = showRegister;
    loginButton.onclick = loginUser;
};
var showRegister = function () {
    var registerDiv = document.querySelector("#registration");
    var loginDiv = document.querySelector("#login");
    loginDiv.style.display = "none";
    registerDiv.style.display = "block";
    var registerButton = document.querySelector("#registerButton")
    registerButton.onclick = getUserInfo;
};
var getUserInfo = function () {
    var usernameField = document.querySelector("#username")
    var username = usernameField.value
    var emailField = document.querySelector("#registeremail")
    var email = emailField.value
    var passwordField = document.querySelector("#registerpassword")
    var password = passwordField.value
    //console.log(username)
    //console.log(email)
    //console.log(password)
    usernameField.value = ("");
    emailField.value = ("");
    passwordField.value = ("");
    createUser(username, email, password);
};

var createUser = function (username, email, password) {
    var data = `username=${encodeURIComponent(username)}`;
    data += `&email=${encodeURIComponent(email)}`;
    data += `&password=${encodeURIComponent(password)}`;
    fetch("https://nate-bens-vgdb.herokuapp.com/users", {
        method: "POST",
        body: data,
        credentials: 'include',
        headers: {
            "content-type": " application/x-www-form-urlencoded"
        }
    }).then(function (response) {
        if (response.status == 201) {
            console.log(username, "Succesfully created");
            loadGames();
        }
        else {
            alert("Registration Failed");
            loadGames();
        }
    });
};

var loginUser = function () {
    var emailField = document.querySelector("#email")
    var email = emailField.value
    var passwordField = document.querySelector("#password")
    var password = passwordField.value
    emailField.value = ("");
    passwordField.value = ("");
    var data = `email=${encodeURIComponent(email)}`;
    data += `&password=${encodeURIComponent(password)}`;
    fetch("https://nate-bens-vgdb.herokuapp.com/sessions", {
        method: "POST",
        body: data,
        credentials: 'include',
        headers: {
            "content-type": " application/x-www-form-urlencoded"
        }
    }).then(function (response) {
        if (response.status == 401) {
            alert("Login Failed")
        }
        else {
            console.log(email, "Logged in");
            loadGames();
        }
    });

};

// Page Load
var loadGames = function () {
    fetch("https://nate-bens-vgdb.herokuapp.com/videogames", {
        credentials: 'include',
    }).then(function (response) {
        console.log(response.status)
        if (response.status == 401) {
            showLogin();
        }
        else {
            response.json().then(function (theGames) {
                console.log("Videogames:", theGames);
                showGames(theGames);
            });
        } 
    });
};
loadGames();
