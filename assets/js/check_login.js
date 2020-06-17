// Javascript to check if an ID token is available. If not, return to the login.
let params = new URLSearchParams(window.location.hash.substr(1));
console.log(params);
var token = params.get('id_token');
//Let's see if the token already exists for this sesson.
if (token == null){
  token = sessionStorage.getItem("id_token");
}
console.log(token);
if (token !== null){
    try {
      var decoded_token = jwt_decode(token);
      console.log(decoded_token);
      if (decoded_token.exp > Date.now() / 1000) {
        sessionStorage.setItem("id_token", token);
        sessionStorage.setItem("email", decoded_token.email)
      }
      else {
        alert("The ID token expired.");
        window.location = "login.html";
      }
    }
    catch {
      alert("There was an error parsing the ID token.");
      window.location = "login.html";
    }
}
else {
  // There is not a token, so let's go get one.
  window.location = "login.html";
}