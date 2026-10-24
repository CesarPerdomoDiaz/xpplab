// The login page's one job: say why the last attempt failed.
const q = new URLSearchParams(location.search);
const error = document.getElementById("error");
if (q.has("wrong")) { error.textContent = "That password is not right."; error.classList.remove("hidden"); }
if (q.has("wait")) { error.textContent = "Too many tries. Wait five minutes and try again."; error.classList.remove("hidden"); }
