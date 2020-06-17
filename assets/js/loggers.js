$( document ).ready(function() {
    $('#email').text(sessionStorage.getItem("email"));
    var access_token = window.sessionStorage.getItem('id_token');
    $.ajax({
        url: "https://47tzdaoo6k.execute-api.us-east-2.amazonaws.com/dev/devices",
        method: "GET",
        crossDomain: true,
        dataType: 'json',
        headers: {
            'Authorization': "Bearer " + access_token,
            'Access-Control-Allow-Origin': "*"
        }
    })
    .done(function(data, textStatus, jqXHR){
        console.log(data)
        for (var line in data) {
            if (data.hasOwnProperty(line)) {
                var device_label = data[line]['device_label'];
                var provision_time = data[line]['provision_time'];
                if (provision_time === undefined){provision_time='Unknown'};
                var id = data[line]['id'];
                var upload_ip = data[line]['upload_ip'];
                if (upload_ip === undefined){upload_ip='Unknown'};
                var upload_time = data[line]['upload_time'];
                if (upload_time === undefined){upload_time='Unknown'};
                $("#loggerID").append(
                    '<tr id="row_' + id + '">' +
                    "<td>" + device_label + "</td>" +
                    "<td>" + id + '</td>' +
                    '<td>' + provision_time.split('T')[0] + "</td>" +
                    '<td>' + upload_time.split('T')[0] + "</td>" +
                    '<td>' + upload_ip + "</td>" +
                    "</tr>"
                );
            }
        }
    })
    .fail(function (error_data){
        displayRibbon('There was an error when retrieving CAN Logger information.', 'danger');
        console.info(error_data)        
    });    
});

// https://www.w3schools.com/howto/howto_js_sort_table.asp
function sortTable(n) {
  var table, rows, switching, i, x, y, shouldSwitch, dir, switchcount = 0;
  table = document.getElementById("myTable2");
  switching = true;
  // Set the sorting direction to ascending:
  dir = "asc";
  /* Make a loop that will continue until
  no switching has been done: */
  while (switching) {
    // Start by saying: no switching is done:
    switching = false;
    rows = table.rows;
    /* Loop through all table rows (except the
    first, which contains table headers): */
    for (i = 1; i < (rows.length - 1); i++) {
      // Start by saying there should be no switching:
      shouldSwitch = false;
      /* Get the two elements you want to compare,
      one from current row and one from the next: */
      x = rows[i].getElementsByTagName("TD")[n];
      y = rows[i + 1].getElementsByTagName("TD")[n];
      /* Check if the two rows should switch place,
      based on the direction, asc or desc: */
      if (dir == "asc") {
        if (x.innerHTML.toLowerCase() > y.innerHTML.toLowerCase()) {
          // If so, mark as a switch and break the loop:
          shouldSwitch = true;
          break;
        }
      } else if (dir == "desc") {
        if (x.innerHTML.toLowerCase() < y.innerHTML.toLowerCase()) {
          // If so, mark as a switch and break the loop:
          shouldSwitch = true;
          break;
        }
      }
    }
    if (shouldSwitch) {
      /* If a switch has been marked, make the switch
      and mark that a switch has been done: */
      rows[i].parentNode.insertBefore(rows[i + 1], rows[i]);
      switching = true;
      // Each time a switch is done, increase this count by 1:
      switchcount ++;
    } else {
      /* If no switching has been done AND the direction is "asc",
      set the direction to "desc" and run the while loop again. */
      if (switchcount == 0 && dir == "asc") {
        dir = "desc";
        switching = true;
      }
    }
  }
}
