function doGet(){
  Logger.log(ContentService.createTextOutput(GetEvents()));
  return ContentService.createTextOutput(GetEvents());
}

function GetEvents(){
  
  var _calendarName = ''
  var Cal = CalendarApp.getCalendarsByName(_calendarName)[0];
  var Mail = GmailApp.search('is:unread label:all AND NOT label:Spam ');

  var Now = new Date();
  var MILLIS_PER_DAY = 1000 * 60 * 60 * 24;
  var Tomorrow = new Date(Now.getTime() + MILLIS_PER_DAY);

  //Logger.log(Now);
  //Logger.log(Tomorrow);

  //Logger.log(Mail.length);
  str = "Unread Mail:";
  str += Mail.length;
  str += '\n';

  var events = Cal.getEventsForDay(Now);
  //Logger.log(events.length);
  str += "Total Events Today:";
  str += String(events.length);
  str += '\n';
  for (var i = 0; i < events.length; i++){
    str += events[i].getTitle() + ';' ;
    if(events[i].getStartTime().getHours() < 10){
      str += '0';
    }
    str += events[i].getStartTime().getHours();
    str += ':'
    if(events[i].getStartTime().getMinutes() < 10){
      str += '0';
    }
    str += events[i].getStartTime().getMinutes() + ';' ;

    if(events[i].getEndTime().getHours() < 10){
      str += '0';
    }
    str += events[i].getEndTime().getHours();
    str += ':'
    if(events[i].getEndTime().getMinutes() < 10){
      str += '0';
    }
    str += events[i].getEndTime().getMinutes() + ';' ;
    str += '\n';
  }


  //Logger.log(str);
  return str;
}