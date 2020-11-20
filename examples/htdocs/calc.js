var calc = new Vue({
  el: '#calc',
  data: {
    value: '0'
  },
  methods: {
    clearLine: function() {
      this.value = '0';
    },
    append: function(value) {
      if (this.value == '0') {
        this.value = value;
      } else {
        this.value += value;
      }
    },
    backspace: function() {
      if (this.value.length > 1) {
        this.value = this.value.substring(0, this.value.length - 1);
      } else {
        this.value = '0';
      }
    },
    calculate: function() {
      var calc = this;
      window.external.invoke("eval:print('calculate', '" + calc.value + "'); callBrowser('result', " + calc.value + ")")
    }
  }
});

function result(value) {
  calc.value = '' + value;
};
