function validateForm() {
   const username = document.getElementById('username').value;
   const password = document.getElementById('password').value;

   let isValid = true;

   // Username validation: 3 - 12 characters, alphanumeric
   const usernameRegex = /^[0-9]{8}$/;
   const usernameError = document.getElementById('username-error');
   if (!usernameRegex.test(username)) {
      usernameError.textContent = '应为8位数字';
      isValid = false;
   } else {
      usernameError.textContent = '';
   }

   // Password validation: At least 6 characters
   const passwordRegex = /^.{6,}$/;
   const passwordError = document.getElementById('password-error');
   if (!passwordRegex.test(password)) {
      passwordError.textContent = '长度至少为6';
      isValid = false;
   } else {
      passwordError.textContent = '';
   }

   return isValid;
}

document.getElementById('login-form').addEventListener('submit', async (event) => {
   event.preventDefault(); // Prevent form submission

   if (!validateForm()) {
      alert("\u{1f605}");
      return;
   }

   const username = document.getElementById('username').value;
   const passwd = document.getElementById('password').value;
   const hashed_passwd = CryptoJS.SHA256(passwd).toString();
   console.log("your hashed_passwd is : " + hashed_passwd);

   try {
      const response = await fetch('/api/login', {
         method: 'POST',
         headers: {
            'Content-Type': 'application/json',
         },
         body: JSON.stringify({
            uID: username,
            uCode: hashed_passwd, // 使用哈希后的密码
         }),
      });

      if (response.ok) {
         const result = await response.json();
         console.log('登录成功:', result);
         localStorage.setItem('username', result.username);
         console.log("your authority_num is: " + result.authority_num);
         localStorage.setItem('authority_num', result.authority_num);
         localStorage.setItem('uID', result.uID);
         alert("登录成功: \u{1f60a}");
         window.location.reload();
      } else {
         const result = await response.json();
         console.log('登录失败: ' + result.message);
         alert('登录失败，请检查用户ID或密码\u{1f605}');
      }
   } catch (error) {
      console.error('请求出错:', error);
      alert('登录时发生错误，请稍后重试');
   } finally {
      // 清空密码框
      document.getElementById('password').value = '';
   }
});

document.getElementById('login-form').addEventListener('input', () => {
   validateForm(); // Validate in real-time as user inputs data
});

function welcomeLoginedUser() {
   document.getElementById('login-box').innerHTML = `
      <span>
         hi, <strong>${localStorage.username}</strong>!
      </span>
      <button style="margin-left: 3px;" onclick="logout()">登出</button>
   `;
}

function logout() {
   localStorage.removeItem('username');
   localStorage.removeItem('authority_num');
   localStorage.removeItem('uID');
   // localStorage.removeItem('uid');
   window.location.reload();
   window.location.href = '/';
}

// change the welcome message for logined user
if (localStorage.getItem('username')) {
   welcomeLoginedUser();
}

// When the user clicks anywhere outside of the modal, close it
var modal = document.getElementById('id01');
window.onclick = function(event) {
   if (event.target == modal) {
      modal.style.display = "none";
   }
}
