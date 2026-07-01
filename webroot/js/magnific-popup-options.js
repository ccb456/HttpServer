$(document).ready(function() {
  // MagnificPopup
  var magnifPopup = function() {
    var popupOptions = {
      type: 'image',
      removalDelay: 300,
      mainClass: 'mfp-with-zoom',
      gallery:{
        enabled:true
      },
      zoom: {
        enabled: true,
        duration: 300,
        easing: 'ease-in-out',
        opener: function(openerElement) {
          return openerElement.is('img') ? openerElement : openerElement.find('img');
        }
      }
    };

    $('.image-popup').magnificPopup(popupOptions);
    $('.popup-gallery').magnificPopup(popupOptions);
  };

  // Call the functions 
  magnifPopup();

});